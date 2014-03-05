#include "QGBTextbank.h"

#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QtCore/QStringList>


#define ENGLISH

QGBTextbank::QGBTextbank(QString rom, QString dump)
{
    txtFile = new QFile(dump);
    romFile = new QFile(rom);
}

QGBTextbank::~QGBTextbank()
{
    delete txtFile;
    delete romFile;
}

void QGBTextbank::dumpText()
{
    romFile->open(QIODevice::ReadOnly);
    txtFile->open(QIODevice::WriteOnly);

    romFile->seek(0x38000);
    txtFile->seek(0);

    readPointers();

    for (quint8 i = 0; i < MAX_STRINGS; i++)
        dumpPointer(i);

    romFile->close();
    txtFile->close();
}

void QGBTextbank::insertText()
{
    romFile->open(QIODevice::ReadWrite);
    txtFile->open(QIODevice::ReadOnly);

    romFile->seek(0x38148);
    txtFile->seek(0);

    QByteArray *currentBlock;

    yesNoBoxCounter = 0;
    txtJumpCounter = 0;

    //there are some bytes after the original end of the text
    //starting at 0x3B15D
    //0xFF starts at 0x3B200 till 0x3C000

    //read in text
    for (int i = 0; i < MAX_STRINGS; i++)
    {
        //read and encode text block
        currentBlock = readTextBlock(txtFile, i);

        if (currentBlock == NULL)
        {
            qDebug() << "no text block returned; aborting...";
            break;
        }

        //dumpByteArray(currentBlock);

        //check destination in rom file
        if (currentBlock->size() + romFile->pos() > 0x3B15D) // does not fit before "bytes"
        {
            if (romFile->pos() <= 0x3B15D)  // we do not fit below 0x3B15C for the first
            {                              // time so we jump to 0x3B200
                romFile->seek(0x3B200);
                qDebug() << "jumping to 0x3B200";
            }

            if (currentBlock->size() + romFile->pos() > 0x3BF70)
            {
                qDebug() << QString("WARNING: not enough space for text block %1").arg(i);
                break;
            }
        }

        //update pointer
        updatePointer(i, romFile->pos());
        qDebug() << QString("block %1; new pointer: %2").arg(i).arg(txtPointers[i], 4, 16, QChar('0'));
        //write encoded block
        writeByteField(currentBlock, romFile);

        delete currentBlock;
    }

    QDataStream out(romFile);
    out.setByteOrder(QDataStream::LittleEndian);

    //correct and write yesNoBox pointer
    for (int i = 0; i < yesNoBoxCounter; i++)
    {
        qDebug() << "textblock:" << yesNoBoxes[i].textBlock << "offset:" << yesNoBoxes[i].offset << "yes:" << yesNoBoxes[i].yes << "no:" << yesNoBoxes[i].no;
        romFile->seek(pointerToOffset(yesNoBoxes[i].textBlock) + yesNoBoxes[i].offset);
        out << txtPointers[yesNoBoxes[i].yes];
        out << txtPointers[yesNoBoxes[i].no];
    }

    //correct and write jump pointer
    for (int i = 0; i < txtJumpCounter; i++)
    {
        qDebug() << "textblock:" << txtJump[i].textBlock << "offset:" << txtJump[i].writeOffset;
        romFile->seek(pointerToOffset(txtJump[i].textBlock) + txtJump[i].writeOffset);
        out << (quint16)(txtPointers[txtJump[i].firstTxtBlock] + txtJump[i].firstTxtBlockOffset);
        out << (quint16)(txtPointers[txtJump[i].secondTxtBlock] + txtJump[i].secondTxtBlockOffset);
    }

    //correct vial->malestrom exchange
    qDebug() << (pointerToOffset(136) + 2);
    romFile->seek(pointerToOffset(136) + 2);
    out << (quint16)(txtPointers[136] + 8);

    //write new pointer table
    romFile->seek(0x38006);
    for (int i = 0; i < MAX_STRINGS; i++)
        out << txtPointers[i];


    romFile->close();
    txtFile->close();
}

QByteArray *QGBTextbank::readTextBlock(QFile *in, quint8 blockNum)
{
    QByteArray *newBlock = new QByteArray();
    QByteArray inBlock;

    QDataStream raw(in);
    QString line;
    quint8 byte;
    quint16 unicode;
    quint8 lastTile = 0x00;
    quint8 currentTile;
    quint8 count;
    bool firstLineBreak = false;

    //check first line: num,0xPointer
    line = "";
    raw >> byte;
    while (byte != 0x0A)
    {
        line += byte;
        raw >> byte;
    }

    //line = txt.readLine();
    if (line.split(",").size() != 2)
    {
        qDebug() << "ERROR: wrong block header!" << line.split(",");
        return NULL;
    }
    //qDebug() << "header" << line;

    //we read until two consecutive line breaks
    do
    {
        raw >> byte;
        if (byte == 0x0A)
        {
            if (firstLineBreak)
                break;
            else
                firstLineBreak = true;
        }
        else
            firstLineBreak = false;

        inBlock.append(byte);
    }
    while (true);

    QString name;

    for (int i = 0; i < inBlock.size(); i++)
    {
        if (inBlock[i] == '<')
        {
            lastTile = 0x00; //make sure that we do not try to DTE encode "over" script commands
            name = "";
            i++;
            while ((inBlock[i] != '>') && (inBlock[i] != ','))
            {
                name += inBlock[i];
                i++;
            }

            if (name == "end")
            {
                newBlock->resize(newBlock->size()-1);
                //qDebug() << "<end> tag found";
                break;
            }

            if ((name != "script?") && (name != "jump"))
            {
                newBlock->append(nameToTile(name));
            }

            if ((name != "yesNoBox") && (name != "jump"))
            {
                while (inBlock[i] == ',')
                {
                    i += 3;
                    name = "";
                    for (int j = 0; j < 2; j++)
                    {
                        name += inBlock[i];
                        i++;
                    }

                    //string 0xname to quint8 and append
                    newBlock->append((quint8)name.toUInt(NULL, 16));
                }
            }
            else if (name == "yesNoBox")        //handling for yesNoBox
            {
                yesNoBoxes[yesNoBoxCounter].offset = newBlock->size();
                yesNoBoxes[yesNoBoxCounter].textBlock = blockNum;
                i++;

                name = "";
                while (inBlock[i] != ',')
                {
                    name += inBlock[i];
                    i++;
                }
                yesNoBoxes[yesNoBoxCounter].yes = name.toUInt();

                name = "";
                i++;
                while (inBlock[i] != '>')
                {
                    name += inBlock[i];
                    i++;
                }
                yesNoBoxes[yesNoBoxCounter].no = name.toUInt();

                byte = 0x00;
                newBlock->append(byte);
                newBlock->append(byte);
                newBlock->append(byte);
                newBlock->append(byte);
                yesNoBoxCounter++;
            }
            else if (name == "jump")       //jump handling
            {
                //qDebug() << "jump handling";

                //first we get the "command" 0xf4 0xf5 0xf8 0xf9 or 0xfa
                i += 3;
                name = "";
                for (int j = 0; j < 2; j++)
                {
                    name += inBlock[i];
                    i++;
                }
                //qDebug() << "cmd" << name;

                byte = (quint8)name.toUInt(NULL, 16);
                newBlock->append(byte);

                //now byte holds the command
                //we now know how many additional bytes are need before the pointer info
                switch (byte)
                {
                    case 0xf2:
                        count = 0;
                        i += 10;
                        byte = 0x00;
                        newBlock->append(byte);
                        newBlock->append(byte);
                    break;
                    case 0xf5: count = 3; break;
                    case 0xf4: case 0xf9: case 0xfa: count = 1; break;
                    case 0xf8: default: count = 0;
                }

                //qDebug() << "count" << count;

                //append these bytes
                for (int t = 0; t < count; t++)
                {
                    i += 3;
                    name = "";
                    for (int j = 0; j < 2; j++)
                    {
                        name += inBlock[i];
                        i++;
                    }

                    //qDebug() << "add byte" << name << "0x" << name.toUInt(NULL, 16);

                    //string 0xname to quint8 and append
                    newBlock->append((quint8)name.toUInt(NULL, 16));
                }

                //now only the two pointers left
                txtJump[txtJumpCounter].textBlock = blockNum;
                txtJump[txtJumpCounter].writeOffset = newBlock->size();

                //pointer1
                i++; //advance behind ","
                name = "";
                while (inBlock[i] != ',')
                {
                    name += inBlock[i];
                    i++;
                }
                txtJump[txtJumpCounter].firstTxtBlock = (quint8)name.toUInt();
                //qDebug() << "first" << txtJump[txtJumpCounter].firstTxtBlock;

                i++; //advance behind ","
                name = "";
                while (inBlock[i] != ',')
                {
                    name += inBlock[i];
                    i++;
                }
                txtJump[txtJumpCounter].firstTxtBlockOffset = (quint16)name.toUInt();
                //qDebug() << "firstOffset" << txtJump[txtJumpCounter].firstTxtBlockOffset;

                //pointer2
                i++; //advance behind ","
                name = "";
                while (inBlock[i] != ',')
                {
                    name += inBlock[i];
                    i++;
                }
                txtJump[txtJumpCounter].secondTxtBlock = (quint8)name.toUInt();
                //qDebug() << "second" << txtJump[txtJumpCounter].secondTxtBlock;

                i++; //advance behind ","
                name = "";
                while (inBlock[i] != '>')
                {
                    name += inBlock[i];
                    i++;
                }
                txtJump[txtJumpCounter].secondTxtBlockOffset = (quint16)name.toUInt();
                //qDebug() << "secondOffset" << txtJump[txtJumpCounter].secondTxtBlockOffset;

                byte = 0x00;
                newBlock->append(byte);
                newBlock->append(byte);
                newBlock->append(byte);
                newBlock->append(byte);
                txtJumpCounter++;

            }
        }
        else        //read unicode
        {
            byte = inBlock[i];
            unicode = byte;
            if (!(byte & 0x80)) //only one byte
            {
                unicode = byte;
            }
            else if ((byte >> 5) == 0x6) //two bytes
            {
                unicode = (byte & 0x1F)*0x40 + ((quint8)inBlock[i+1] & 0x3F);
                i++;
            }
            else if((byte >> 4) == 0xE) // three bytes
            {
                unicode = (byte & 0xF) * 0x1000 + ((quint8)inBlock[i+1] & 0x3F) * 0x40 + ((quint8)inBlock[i+2] & 0x3F);
                i += 2;
            }

            currentTile = unicodeToTile(unicode);
#ifdef ENGLISH
            if (getDTE(currentTile, lastTile))                  //we have a DTE encoding
            {
                lastTile = 0x00;
                (*newBlock)[newBlock->size()-1] = currentTile; //replace last tile with DTE - currentTile is changed by getDTE!
            }
            else                                                // no DTE
#endif
            {
                lastTile = currentTile;
                newBlock->append(currentTile);
            }
        }
    }
    return newBlock;
}

bool QGBTextbank::getDTE(quint8 &currentTile, quint8 lastTile)
{
    if (lastTile == 0x00)
        return false;

    if ((lastTile == 0x5a) && (currentTile == 0x4e)) { currentTile = 0xB0; return true; }
    if ((lastTile == 0x4e) && (currentTile == 0x4b)) { currentTile = 0xB1; return true; }
    if ((lastTile == 0x58) && (currentTile == 0x4b)) { currentTile = 0xB2; return true; }
    if ((lastTile == 0x47) && (currentTile == 0x54)) { currentTile = 0xB3; return true; }
    if ((lastTile == 0x55) && (currentTile == 0x5b)) { currentTile = 0xB4; return true; }
    if ((lastTile == 0x4f) && (currentTile == 0x54)) { currentTile = 0xB5; return true; }
    if ((lastTile == 0x4b) && (currentTile == 0x58)) { currentTile = 0xB6; return true; }
    if ((lastTile == 0x54) && (currentTile == 0x4a)) { currentTile = 0xB7; return true; }
    if ((lastTile == 0x5e) && (currentTile == 0x4c)) { currentTile = 0xB8; return true; }
    if ((lastTile == 0x5e) && (currentTile == 0x4b)) { currentTile = 0xB9; return true; }
    if ((lastTile == 0x5f) && (currentTile == 0x55)) { currentTile = 0xBA; return true; }
    if ((lastTile == 0x52) && (currentTile == 0x4b)) { currentTile = 0xBB; return true; }
    if ((lastTile == 0x5a) && (currentTile == 0x55)) { currentTile = 0xBC; return true; }
    if ((lastTile == 0x54) && (currentTile == 0x4d)) { currentTile = 0xBD; return true; }
    if ((lastTile == 0x47) && (currentTile == 0x52)) { currentTile = 0xBE; return true; }
    if ((lastTile == 0x4b) && (currentTile == 0x47)) { currentTile = 0xBF; return true; }
    if ((lastTile == 0x58) && (currentTile == 0x47)) { currentTile = 0xC0; return true; }
    if ((lastTile == 0x48) && (currentTile == 0x58)) { currentTile = 0xC1; return true; }
    if ((lastTile == 0x5c) && (currentTile == 0x4b)) { currentTile = 0xC2; return true; }
    if ((lastTile == 0x4f) && (currentTile == 0x59)) { currentTile = 0xC3; return true; }
    if ((lastTile == 0x4b) && (currentTile == 0x4a)) { currentTile = 0xC4; return true; }
    if ((lastTile == 0x4c) && (currentTile == 0x4f)) { currentTile = 0xC5; return true; }
    if ((lastTile == 0x4f) && (currentTile == 0x58)) { currentTile = 0xC6; return true; }
    if ((lastTile == 0x47) && (currentTile == 0x58)) { currentTile = 0xC7; return true; }
    if ((lastTile == 0x59) && (currentTile == 0x4b)) { currentTile = 0xC8; return true; }
    if ((lastTile == 0x55) && (currentTile == 0x58)) { currentTile = 0xC9; return true; }
    if ((lastTile == 0x4b) && (currentTile == 0x48)) { currentTile = 0xCA; return true; }
    if ((lastTile == 0x55) && (currentTile == 0x4c)) { currentTile = 0xCB; return true; }
    if ((lastTile == 0x4e) && (currentTile == 0x4f)) { currentTile = 0xCC; return true; }
    if ((lastTile == 0x59) && (currentTile == 0x5a)) { currentTile = 0xCD; return true; }
    if ((lastTile == 0x55) && (currentTile == 0x5d)) { currentTile = 0xCE; return true; }
    if ((lastTile == 0x55) && (currentTile == 0x54)) { currentTile = 0xCF; return true; }
    if ((lastTile == 0x49) && (currentTile == 0x47)) { currentTile = 0xD0; return true; }
    if ((lastTile == 0x4c) && (currentTile == 0x4b)) { currentTile = 0xD1; return true; }
    if ((lastTile == 0x4d) && (currentTile == 0x4e)) { currentTile = 0xD2; return true; }
    if ((lastTile == 0x4e) && (currentTile == 0x47)) { currentTile = 0xD3; return true; }
    if ((lastTile == 0x4f) && (currentTile == 0x5a)) { currentTile = 0xD4; return true; }
    if ((lastTile == 0x4b) && (currentTile == 0x59)) { currentTile = 0xD5; return true; }
    if ((lastTile == 0x4b) && (currentTile == 0x54)) { currentTile = 0xD6; return true; }
    if ((lastTile == 0x52) && (currentTile == 0x47)) { currentTile = 0xD7; return true; }
    if ((lastTile == 0x52) && (currentTile == 0x52)) { currentTile = 0xD8; return true; }
    if ((lastTile == 0x47) && (currentTile == 0x5a)) { currentTile = 0xD9; return true; }
    if ((lastTile == 0x4e) && (currentTile == 0x55)) { currentTile = 0xDA; return true; }
    if ((lastTile == 0x48) && (currentTile == 0x4b)) { currentTile = 0xDB; return true; }
    if ((lastTile == 0x4b) && (currentTile == 0x49)) { currentTile = 0xDC; return true; }
    if ((lastTile == 0x53) && (currentTile == 0x47)) { currentTile = 0xDD; return true; }
    if ((lastTile == 0x5a) && (currentTile == 0x58)) { currentTile = 0xDE; return true; }
    if ((lastTile == 0x58) && (currentTile == 0x55)) { currentTile = 0xDF; return true; }
    if ((lastTile == 0x54) && (currentTile == 0x4b)) { currentTile = 0xE0; return true; }
    if ((lastTile == 0x53) && (currentTile == 0x4b)) { currentTile = 0xE1; return true; }
    if ((lastTile == 0x55) && (currentTile == 0x53)) { currentTile = 0xE2; return true; }

    return false;
}

quint8 QGBTextbank::unicodeToTile(quint16 unicode)
{
#ifdef ENGLISH
    switch (unicode)
    {
    case 0x0020: return 0x00; // space
    case 0x0030: return 0x34;  //0
    case 0x0031: return 0x35;  //1
    case 0x0032: return 0x36;  //2
    case 0x0033: return 0x37;  //3
    case 0x0034: return 0x38;  //4
    case 0x0035: return 0x39;  //5
    case 0x0036: return 0x3A;  //6
    case 0x0037: return 0x3B;  //7
    case 0x0038: return 0x3C;  //8
    case 0x0039: return 0x3D;  //9
    case 0x0041: return 0x47;  //A
    case 0x0042: return 0x48;  //B
    case 0x0043: return 0x49;  //C
    case 0x0044: return 0x4A;  //D
    case 0x0045: return 0x4B;  //E
    case 0x0046: return 0x4C;  //F
    case 0x0047: return 0x4D;  //G
    case 0x0048: return 0x4E;  //H
    case 0x0049: return 0x4F;  //I
    case 0x004A: return 0x50;  //J
    case 0x004B: return 0x51;  //K
    case 0x004C: return 0x52;  //L
    case 0x004D: return 0x53;  //M
    case 0x004E: return 0x54;  //N
    case 0x004F: return 0x55;  //O
    case 0x0050: return 0x56;  //P
    case 0x0051: return 0x57;  //Q
    case 0x0052: return 0x58;  //R
    case 0x0053: return 0x59;  //S
    case 0x0054: return 0x5A;  //T
    case 0x0055: return 0x5B;  //U
    case 0x0056: return 0x5C;  //V
    case 0x0057: return 0x5D;  //W
    case 0x0058: return 0x5E;  //X
    case 0x0059: return 0x5F;  //Y
    case 0x005A: return 0x60;  //Z
//lower case
    case 0x0061: return 0x61;  //A
    case 0x0062: return 0x62;  //B
    case 0x0063: return 0x63;  //C
    case 0x0064: return 0x64;  //D
    case 0x0065: return 0x65;  //E
    case 0x0066: return 0x66;  //F
    case 0x0067: return 0x67;  //G
    case 0x0068: return 0x68;  //H
    case 0x0069: return 0x69;  //I
    case 0x006A: return 0x6A;  //J
    case 0x006B: return 0x6B;  //K
    case 0x006C: return 0x6C;  //L
    case 0x006D: return 0x6D;  //M
    case 0x006E: return 0x6E;  //N
    case 0x006F: return 0x6F;  //O
    case 0x0070: return 0x70;  //P
    case 0x0071: return 0x71;  //Q
    case 0x0072: return 0x72;  //R
    case 0x0073: return 0x73;  //S
    case 0x0074: return 0x74;  //T
    case 0x0075: return 0x75;  //U
    case 0x0076: return 0x76;  //V
    case 0x0077: return 0x77;  //W
    case 0x0078: return 0x78;  //X
    case 0x0079: return 0x79;  //Y
    case 0x007A: return 0x7A;  //Z

    case 0x002E: return 0x3F;  //.
    case 0x002C: return 0x43;  //,
    case 0x0027: return 0x44;  //'
    case 0x003A: return 0x44;  //:
    case 0x201C: return 0x40;  //"
    case 0x201D: return 0x41;  //,,
    case 0x0021: return 0x45;  //!
    case 0x003F: return 0x46;  //?

    case 0x002D: return 0xA0;  //-

    case 0x000A: return 0xE3;  //line break

    default: qDebug() << "WARNING unhandeld unicode!" << unicode; return 0x00;
    }
#else
    switch (unicode)
    {
    case 0x0020: return 0x00; // space
    case 0x003A: return 0x01; //:
    case 0x309A: return 0x02; // ゙
    case 0x3099: return 0x03; // ゙
    case 0x3041: return 0x04; //ぁ
    case 0x3043: return 0x05; //ぃ
    case 0x3045: return 0x06; //ぅ
    case 0x3047: return 0x07; //ぇ
    case 0x3049: return 0x08; //ぉ
    case 0x3083: return 0x09; //ゃ
    case 0x3085: return 0x0A; //ゅ
    case 0x3087: return 0x0B; //ょ
    case 0x3063: return 0x0C; //っ
    case 0x300C: return 0x0D; //「
    case 0x300D: return 0x0E; //」
    case 0x0027: return 0x0F; //'
    case 0x0030: return 0x10;  //0
    case 0x0031: return 0x11;  //1
    case 0x0032: return 0x12;  //2
    case 0x0033: return 0x13;  //3
    case 0x0034: return 0x14;  //4
    case 0x0035: return 0x15;  //5
    case 0x0036: return 0x16;  //6
    case 0x0037: return 0x17;  //7
    case 0x0038: return 0x18;  //8
    case 0x0039: return 0x19;  //9
    case 0x0041: return 0x1A;  //A
    case 0x0042: return 0x1B;  //B
    case 0x0043: return 0x1C;  //C
    case 0x0044: return 0x1D;  //D
    case 0x0045: return 0x1E;  //E
    case 0x0046: return 0x1F;  //F
    case 0x0047: return 0x20;  //G
    case 0x0048: return 0x21;  //H
    case 0x0049: return 0x22;  //I
    case 0x004A: return 0x23;  //J
    case 0x004B: return 0x24;  //K
    case 0x004C: return 0x25;  //L
    case 0x004D: return 0x26;  //M
    case 0x004E: return 0x27;  //N
    case 0x004F: return 0x28;  //O
    case 0x0050: return 0x29;  //P
    case 0x0051: return 0x2A;  //Q
    case 0x0052: return 0x2B;  //R
    case 0x0053: return 0x2C;  //S
    case 0x0054: return 0x2D;  //T
    case 0x0055: return 0x2E;  //U
    case 0x0056: return 0x2F;  //V
    case 0x0057: return 0x30;  //W
    case 0x0058: return 0x31;  //X
    case 0x0059: return 0x32;  //Y
    case 0x005A: return 0x33;  //Z
    case 0x30A1: return 0x34;  //ァ
    case 0x30A3: return 0x35;  //ィ
    case 0x30A5: return 0x36;  //ゥ
    case 0x30A7: return 0x37;  //ェ
    case 0x30A9: return 0x38;  //ォ
    case 0x30E3: return 0x39;  //ャ
    case 0x30E5: return 0x3A;  //ュ
    case 0x30E7: return 0x3B;  //ョ
    case 0x30C3: return 0x3C;  //ッ
    case 0x309C: return 0x3D;  //゜
    case 0x309B: return 0x3E;  //゛
    case 0x002E: return 0x3F;  //.
    case 0x3042: return 0x40;  //あ
    case 0x3044: return 0x41;  //い
    case 0x3046: return 0x42;  //う
    case 0x3048: return 0x43;  //え
    case 0x304A: return 0x44;  //お
    case 0x304B: return 0x45;  //か
    case 0x304D: return 0x46;  //き
    case 0x304F: return 0x47;  //く
    case 0x3051: return 0x48;  //け
    case 0x3053: return 0x49;  //こ
    case 0x3055: return 0x4A;  //さ
    case 0x3057: return 0x4B;  //し
    case 0x3059: return 0x4C;  //す
    case 0x305B: return 0x4D;  //せ
    case 0x305D: return 0x4E;  //そ
    case 0x305F: return 0x4F;  //た
    case 0x3061: return 0x50;  //ち
    case 0x3064: return 0x51;  //つ
    case 0x3066: return 0x52;  //て
    case 0x3068: return 0x53;  //と
    case 0x306A: return 0x54;  //な
    case 0x306B: return 0x55;  //に
    case 0x306C: return 0x56;  //ぬ
    case 0x306D: return 0x57;  //ね
    case 0x306E: return 0x58;  //の
    case 0x306F: return 0x59;  //は
    case 0x3072: return 0x5A;  //ひ
    case 0x3075: return 0x5B;  //ふ
    case 0x3078: return 0x5C;  //へ
    case 0x307B: return 0x5D;  //ほ
    case 0x307E: return 0x5E;  //ま
    case 0x307F: return 0x5F;  //み
    case 0x3080: return 0x60;  //む
    case 0x3081: return 0x61;  //め
    case 0x3082: return 0x62;  //も
    case 0x3084: return 0x63;  //や
    case 0x3086: return 0x64;  //ゆ
    case 0x3088: return 0x65;  //よ
    case 0x3089: return 0x66;  //ら
    case 0x308A: return 0x67;  //り
    case 0x308B: return 0x68;  //る
    case 0x308C: return 0x69;  //れ
    case 0x308D: return 0x6A;  //ろ
    case 0x308F: return 0x6B;  //わ
    case 0x3092: return 0x6C;  //を
    case 0x3093: return 0x6D;  //ん
    case 0x0021: return 0x6E;  //!
    case 0x003F: return 0x6F;  //?
    case 0x30A2: return 0x70;  //ア
    case 0x30A4: return 0x71;  //イ
    case 0x30A6: return 0x72;  //ウ
    case 0x30A8: return 0x73;  //エ
    case 0x30AA: return 0x74;  //オ
    case 0x30AB: return 0x75;  //カ
    case 0x30AD: return 0x76;  //キ
    case 0x30AF: return 0x77;  //ク
    case 0x30B1: return 0x78;  //ケ
    case 0x30B3: return 0x79;  //コ
    case 0x30B5: return 0x7A;  //サ
    case 0x30B7: return 0x7B;  //シ
    case 0x30B9: return 0x7C;  //ス
    case 0x30BB: return 0x7D;  //セ
    case 0x30BD: return 0x7E;  //ソ
    case 0x30BF: return 0x7F;  //タ
    case 0x30C1: return 0x80;  //チ
    case 0x30C4: return 0x81;  //ツ
    case 0x30C6: return 0x82;  //テ
    case 0x30C8: return 0x83;  //ト
    case 0x30CA: return 0x84;  //ナ
    case 0x30CB: return 0x85;  //ニ
    case 0x30CC: return 0x86;  //ヌ
    case 0x30CD: return 0x87;  //ネ
    case 0x30CE: return 0x88;  //ノ
    case 0x30CF: return 0x89;  //ハ
    case 0x30D2: return 0x8A;  //ヒ
    case 0x30D5: return 0x8B;  //フ
    case 0x30D8: return 0x8C;  //ヘ
    case 0x30DB: return 0x8D;  //ホ
    case 0x30DE: return 0x8E;  //マ
    case 0x30DF: return 0x8F;  //ミ
    case 0x30E0: return 0x90;  //ム
    case 0x30E1: return 0x91;  //メ
    case 0x30E2: return 0x92;  //モ
    case 0x30E4: return 0x93;  //ヤ
    case 0x30E6: return 0x94;  //ユ
    case 0x30E8: return 0x95;  //ヨ
    case 0x30E9: return 0x96;  //ラ
    case 0x30EA: return 0x97;  //リ
    case 0x30EB: return 0x98;  //ル
    case 0x30EC: return 0x99;  //レ
    case 0x30ED: return 0x9A;  //ロ
    case 0x30EF: return 0x9B;  //ワ
    case 0x30F2: return 0x9C;  //ヲ
    case 0x30F3: return 0x9D;  //ン
    case 0x25B6: return 0x9E;  //▶
    case 0x25BC: return 0x9F;  //▼
    case 0x002D: return 0xA0;  //-
    case 0x002B: return 0xA1;  //+
    case 0x002F: return 0xA2;  // /
    case 0x00D7: return 0xA3; //×
    case 0x25B2: return 0xA4; //▲
    case 0x304C: return 0xB0;   //が
    case 0x304E: return 0xB1;   //ぎ
    case 0x3050: return 0xB2;   //ぐ
    case 0x3052: return 0xB3;   //げ
    case 0x3054: return 0xB4;   //ご
    case 0x3056: return 0xB5;   //ざ
    case 0x3058: return 0xB6;   //じ
    case 0x305A: return 0xB7;   //ず
    case 0x305C: return 0xB8;   //ぜ
    case 0x305E: return 0xB9;   //ぞ
    case 0x3060: return 0xBA;   //だ
    case 0x3062: return 0xBB;   //ぢ
    case 0x3065: return 0xBC;   //づ
    case 0x3067: return 0xBD;   //で
    case 0x3069: return 0xBE;   //ど
    case 0x3070: return 0xBF;   //ば
    case 0x3073: return 0xC0;   //び
    case 0x3076: return 0xC1;   //ぶ
    case 0x3079: return 0xC2;   //べ
    case 0x307C: return 0xC3;   //ぼ
    case 0x30AC: return 0xC4;   //ガ
    case 0x30AE: return 0xC5;   //ギ
    case 0x30B0: return 0xC6;   //グ
    case 0x30B2: return 0xC7;   //ゲ
    case 0x30B4: return 0xC8;   //ゴ
    case 0x30B6: return 0xC9;   //ザ
    case 0x30B8: return 0xCA;   //ジ
    case 0x30BA: return 0xCB;   //ズ
    case 0x30BC: return 0xCC;   //ゼ
    case 0x30BE: return 0xCD;   //ゾ
    case 0x30C0: return 0xCE;   //ダ
    case 0x30C2: return 0xCF;   //ヂ
    case 0x30C5: return 0xD0;   //ヅ
    case 0x30C7: return 0xD1;   //デ
    case 0x30C9: return 0xD2;   //ド
    case 0x30D0: return 0xD3;   //バ
    case 0x30D3: return 0xD4;   //ビ
    case 0x30D6: return 0xD5;   //ブ
    case 0x30D9: return 0xD6;   //ベ
    case 0x30DC: return 0xD7;   //ボ
    case 0x30F4: return 0xD8;   //ヴ
    case 0x3071: return 0xD9;   //ぱ
    case 0x3074: return 0xDA;   //ぴ
    case 0x3077: return 0xDB;   //ぷ
    case 0x307A: return 0xDC;   //ぺ
    case 0x307D: return 0xDD;   //ぽ
    case 0x30D1: return 0xDE;   //パ
    case 0x30D4: return 0xDF;   //ピ
    case 0x30D7: return 0xE0;   //プ
    case 0x30DA: return 0xE1;   //ペ
    case 0x30DD: return 0xE2;   //ポ
    case 0x000A: return 0xE3;  //line break
    default: qDebug() << "WARNING unhandeld unicode!" << unicode; return 0x00;
    }
#endif
}

quint8 QGBTextbank::nameToTile(QString str)
{
    if (str == "br")
        return 0xE3;
    else if (str == "clr")
        return 0xE7;
    else if (str == "press")
        return 0xE4;
    else if (str == "closeTextBox")
        return 0xE5;
    else if (str == "currentPassword")
        return 0xF6;
    else if (str == "inserts item?")
        return 0xEF;
    else if (str == "inserts number?")
        return 0xF7;
    else if (str == "shortWait")
        return 0xE6;
    else if (str == "playSnd")
        return 0xE8;
    else if (str == "yesNoBox")
        return 0xEB;

    qDebug() << "WARNING: unhandled string";
    return 0x00;
}

void QGBTextbank::writeByteField(QByteArray *byteArray, QFile *outFile)
{
    QDataStream out(outFile);

    for (int i = 0; i < byteArray->size(); i++)
        out << (quint8)byteArray->at(i);
}

quint32 QGBTextbank::pointerToOffset(int i)
{
    if (i == MAX_STRINGS)
        return 0x3B15D;
    if (i > MAX_STRINGS)
        return 0;

    return txtPointers[i] + 0x34000;
}

void QGBTextbank::readPointers()
{
    QDataStream in(romFile);
    romFile->seek(0x38006);

    in.setByteOrder(QDataStream::LittleEndian);

    for (int i = 0; i < MAX_STRINGS; i++)
        in >> txtPointers[i];
}

bool QGBTextbank::updatePointer(int i, quint32 newOffset)
{
    if (newOffset > 0x3C000)
        return false;

    txtPointers[i] = newOffset - 0x34000;
    return true;
}

void QGBTextbank::writeUnicode(quint16 unicodeNum)
{
    quint8 highest, high, low;
    QDataStream out(txtFile);

    if (unicodeNum <= 0x7F)
    {
        low = unicodeNum & 0xFF;
        out << low;
    }
    else if (unicodeNum <= 0x7FF)
    {
        high = 0xC0 | ((unicodeNum & 0x07C0) >> 6);
        low = 0x80 | (unicodeNum & 0x3F);
        out << high;
        out << low;
    }
    else// if (unicodeNum <= 0xFFFF)
    {
        highest = 0xE0 | (unicodeNum >> 12);
        high =  0x80 | ((unicodeNum & 0x0FC0) >> 6);
        low = 0x80 | (unicodeNum & 0x3F);
        out << highest;
        out << high;
        out << low;
    }
}

void QGBTextbank::writePlainText(QString text)
{
    QTextStream out(txtFile);
    out << text;
}

void QGBTextbank::writePlainText(QString text, quint8 additionalBytes)
{
    quint8 byte;
    QDataStream in(romFile);
    QString hexString = "";
    quint16 pointerYes, pointerNo, pointer1, pointer2;
    quint8 i;

    if (text == "yesNoBox")
    {
        in.setByteOrder(QDataStream::LittleEndian);
        in >> pointerYes;
        in >> pointerNo;
        hexString = "<yesNoBox,";
        for (i = 0; i < MAX_STRINGS; i++)
            if (pointerYes == txtPointers[i])
            {
                hexString += QString("%1,").arg(i);
                break;
            }
        if (i == MAX_STRINGS)
            qDebug() << "ERROR: could not determine yesNoBox pointer!";

        for (i = 0; i < MAX_STRINGS; i++)
            if (pointerNo == txtPointers[i])
            {
                hexString += QString("%1>").arg(i);
                break;
            }

        if (i == MAX_STRINGS)
            qDebug() << "ERROR: could not determine yesNoBox pointer!";

        writePlainText(hexString);
        return;

    }

    if (text.startsWith("jump"))
    {
        hexString = "<" +  text;
        in.setByteOrder(QDataStream::LittleEndian);

        // condition bytes or whatever
        if (additionalBytes > 4)
            for (int i = 0; i < additionalBytes - 4; i++)
            {
                hexString += ",0x";
                in >> byte;
                hexString += QString("%1").arg(byte, 2, 16, QChar('0'));
            }

        //now we get two pointers
        in >> pointer1;
        in >> pointer2;

        //qDebug() << QString("pointer1 %1 pointer 2 %2").arg(pointer1, 4, 16, QChar('0')).arg(pointer2, 4, 16, QChar('0'));

        //qDebug() << QString("%1").arg(txtPointers[30], 4, 16, QChar('0'));

        //now we need to check their destination
        //pointer1
        for (i = 0; i < MAX_STRINGS; i++)
            if (pointer1 <= txtPointers[i])
            {
                if (pointer1 == txtPointers[i])
                    hexString += QString(",%1,0,").arg(i);
                else
                {
                    hexString += QString(",%1,").arg(i-1);
                    hexString += QString("%1,").arg(pointer1 - txtPointers[i-1]);
                }
                break;
            }
        if (i == MAX_STRINGS)
            qDebug() << "ERROR: could not determine jump destination!";

        //pointer2
        for (i = 0; i < MAX_STRINGS; i++)
            if (pointer2 <= txtPointers[i])
            {
                if (pointer2 == txtPointers[i])
                    hexString += QString("%1,0>").arg(i);
                else
                {
                    hexString += QString("%1,").arg(i-1);
                    hexString += QString("%1>").arg(pointer2 - txtPointers[i-1]);
                }
                break;
            }
        if (i == MAX_STRINGS)
            qDebug() << "ERROR: could not determine jump destination!";

        writePlainText(hexString);
        return;
    }

    for (int i = 0; i < additionalBytes; i++)
    {
        hexString += ",0x";
        in >> byte;
        hexString += QString("%1").arg(byte, 2, 16, QChar('0'));
    }

    writePlainText("<" + text + hexString + ">");
}

void QGBTextbank::dumpPointer(int i)
{
    if (i >= MAX_STRINGS)
        return;

    quint32 offset = pointerToOffset(i);
    quint16 length = pointerToOffset(i+1) - offset;

    qDebug() << QString("Num %1: Offset 0x%2 Length: %3").arg(i).arg(offset, 8, 16, QChar('0')).arg(length);

    romFile->seek(offset);
    QDataStream in(romFile);
    quint8 byte;

    writePlainText(QString("%1,0x%2").arg(i).arg(offset, 6, 16, QChar('0')));

    //for compatibility with posted text dump
    //writePlainText(QString("%1,0x%2").arg(i).arg(offset-0x38000, 4, 16, QChar('0')));
    writeUnicode(0x000A);

    quint16 j = 0;
    while (j < length)
    {
        in >> byte;
        j += writeDecodedTile(byte);
    };

    writeUnicode(0x000A);
    writePlainText("<end>");
    writeUnicode(0x000A);
    writeUnicode(0x000A);
}

quint8 QGBTextbank::writeDecodedTile(quint8 tileNum)
{
    //still unsure: 0xF7 (inserts number); 0xEF (inserts アルゴブのつぼ item? GT: "Argob's Pot")
    switch (tileNum)
    {
    case 0x00: writeUnicode(0x0020); return 1; // space
    case 0x01: writeUnicode(0x003A); return 1; //:
    case 0x02: writeUnicode(0x309A); return 1; // ゙
    case 0x03: writeUnicode(0x3099); return 1; // ゙
    case 0x04: writeUnicode(0x3041); return 1; //ぁ
    case 0x05: writeUnicode(0x3043); return 1; //ぃ
    case 0x06: writeUnicode(0x3045); return 1; //ぅ
    case 0x07: writeUnicode(0x3047); return 1; //ぇ
    case 0x08: writeUnicode(0x3049); return 1; //ぉ
    case 0x09: writeUnicode(0x3083); return 1; //ゃ
    case 0x0A: writeUnicode(0x3085); return 1; //ゅ
    case 0x0B: writeUnicode(0x3087); return 1; //ょ
    case 0x0C: writeUnicode(0x3063); return 1; //っ
    case 0x0D: writeUnicode(0x300C); return 1; //「
    case 0x0E: writeUnicode(0x300D); return 1; //」
    case 0x0F: writeUnicode(0x0027); return 1; //'
    case 0x10: writeUnicode(0x0030); return 1;  //0
    case 0x11: writeUnicode(0x0031); return 1;  //1
    case 0x12: writeUnicode(0x0032); return 1;  //2
    case 0x13: writeUnicode(0x0033); return 1;  //3
    case 0x14: writeUnicode(0x0034); return 1;  //4
    case 0x15: writeUnicode(0x0035); return 1;  //5
    case 0x16: writeUnicode(0x0036); return 1;  //6
    case 0x17: writeUnicode(0x0037); return 1;  //7
    case 0x18: writeUnicode(0x0038); return 1;  //8
    case 0x19: writeUnicode(0x0039); return 1;  //9
    case 0x1A: writeUnicode(0x0041); return 1;  //A
    case 0x1B: writeUnicode(0x0042); return 1;  //B
    case 0x1C: writeUnicode(0x0043); return 1;  //C
    case 0x1D: writeUnicode(0x0044); return 1;  //D
    case 0x1E: writeUnicode(0x0045); return 1;  //E
    case 0x1F: writeUnicode(0x0046); return 1;  //F
    case 0x20: writeUnicode(0x0047); return 1;  //G
    case 0x21: writeUnicode(0x0048); return 1;  //H
    case 0x22: writeUnicode(0x0049); return 1;  //I
    case 0x23: writeUnicode(0x004A); return 1;  //J
    case 0x24: writeUnicode(0x004B); return 1;  //K
    case 0x25: writeUnicode(0x004C); return 1;  //L
    case 0x26: writeUnicode(0x004D); return 1;  //M
    case 0x27: writeUnicode(0x004E); return 1;  //N
    case 0x28: writeUnicode(0x004F); return 1;  //O
    case 0x29: writeUnicode(0x0050); return 1;  //P
    case 0x2A: writeUnicode(0x0051); return 1;  //Q
    case 0x2B: writeUnicode(0x0052); return 1;  //R
    case 0x2C: writeUnicode(0x0053); return 1;  //S
    case 0x2D: writeUnicode(0x0054); return 1;  //T
    case 0x2E: writeUnicode(0x0055); return 1;  //U
    case 0x2F: writeUnicode(0x0056); return 1;  //V
    case 0x30: writeUnicode(0x0057); return 1;  //W
    case 0x31: writeUnicode(0x0058); return 1;  //X
    case 0x32: writeUnicode(0x0059); return 1;  //Y
    case 0x33: writeUnicode(0x005A); return 1;  //Z
    case 0x34: writeUnicode(0x30A1); return 1;  //ァ
    case 0x35: writeUnicode(0x30A3); return 1;  //ィ
    case 0x36: writeUnicode(0x30A5); return 1;  //ゥ
    case 0x37: writeUnicode(0x30A7); return 1;  //ェ
    case 0x38: writeUnicode(0x30A9); return 1;  //ォ
    case 0x39: writeUnicode(0x30E3); return 1;  //ャ
    case 0x3A: writeUnicode(0x30E5); return 1;  //ュ
    case 0x3B: writeUnicode(0x30E7); return 1;  //ョ
    case 0x3C: writeUnicode(0x30C3); return 1;  //ッ
    case 0x3D: writeUnicode(0x309C); return 1;  //゜
    case 0x3E: writeUnicode(0x309B); return 1;  //゛
    case 0x3F: writeUnicode(0x002E); return 1;  //.
    case 0x40: writeUnicode(0x3042); return 1;  //あ
    case 0x41: writeUnicode(0x3044); return 1;  //い
    case 0x42: writeUnicode(0x3046); return 1;  //う
    case 0x43: writeUnicode(0x3048); return 1;  //え
    case 0x44: writeUnicode(0x304A); return 1;  //お
    case 0x45: writeUnicode(0x304B); return 1;  //か
    case 0x46: writeUnicode(0x304D); return 1;  //き
    case 0x47: writeUnicode(0x304F); return 1;  //く
    case 0x48: writeUnicode(0x3051); return 1;  //け
    case 0x49: writeUnicode(0x3053); return 1;  //こ
    case 0x4A: writeUnicode(0x3055); return 1;  //さ
    case 0x4B: writeUnicode(0x3057); return 1;  //し
    case 0x4C: writeUnicode(0x3059); return 1;  //す
    case 0x4D: writeUnicode(0x305B); return 1;  //せ
    case 0x4E: writeUnicode(0x305D); return 1;  //そ
    case 0x4F: writeUnicode(0x305F); return 1;  //た
    case 0x50: writeUnicode(0x3061); return 1;  //ち
    case 0x51: writeUnicode(0x3064); return 1;  //つ
    case 0x52: writeUnicode(0x3066); return 1;  //て
    case 0x53: writeUnicode(0x3068); return 1;  //と
    case 0x54: writeUnicode(0x306A); return 1;  //な
    case 0x55: writeUnicode(0x306B); return 1;  //に
    case 0x56: writeUnicode(0x306C); return 1;  //ぬ
    case 0x57: writeUnicode(0x306D); return 1;  //ね
    case 0x58: writeUnicode(0x306E); return 1;  //の
    case 0x59: writeUnicode(0x306F); return 1;  //は
    case 0x5A: writeUnicode(0x3072); return 1;  //ひ
    case 0x5B: writeUnicode(0x3075); return 1;  //ふ
    case 0x5C: writeUnicode(0x3078); return 1;  //へ
    case 0x5D: writeUnicode(0x307B); return 1;  //ほ
    case 0x5E: writeUnicode(0x307E); return 1;  //ま
    case 0x5F: writeUnicode(0x307F); return 1;  //み
    case 0x60: writeUnicode(0x3080); return 1;  //む
    case 0x61: writeUnicode(0x3081); return 1;  //め
    case 0x62: writeUnicode(0x3082); return 1;  //も
    case 0x63: writeUnicode(0x3084); return 1;  //や
    case 0x64: writeUnicode(0x3086); return 1;  //ゆ
    case 0x65: writeUnicode(0x3088); return 1;  //よ
    case 0x66: writeUnicode(0x3089); return 1;  //ら
    case 0x67: writeUnicode(0x308A); return 1;  //り
    case 0x68: writeUnicode(0x308B); return 1;  //る
    case 0x69: writeUnicode(0x308C); return 1;  //れ
    case 0x6A: writeUnicode(0x308D); return 1;  //ろ
    case 0x6B: writeUnicode(0x308F); return 1;  //わ
    case 0x6C: writeUnicode(0x3092); return 1;  //を
    case 0x6D: writeUnicode(0x3093); return 1;  //ん
    case 0x6E: writeUnicode(0x0021); return 1;  //!
    case 0x6F: writeUnicode(0x003F); return 1;  //?
    case 0x70: writeUnicode(0x30A2); return 1;  //ア
    case 0x71: writeUnicode(0x30A4); return 1;  //イ
    case 0x72: writeUnicode(0x30A6); return 1;  //ウ
    case 0x73: writeUnicode(0x30A8); return 1;  //エ
    case 0x74: writeUnicode(0x30AA); return 1;  //オ
    case 0x75: writeUnicode(0x30AB); return 1;  //カ
    case 0x76: writeUnicode(0x30AD); return 1;  //キ
    case 0x77: writeUnicode(0x30AF); return 1;  //ク
    case 0x78: writeUnicode(0x30B1); return 1;  //ケ
    case 0x79: writeUnicode(0x30B3); return 1;  //コ
    case 0x7A: writeUnicode(0x30B5); return 1;  //サ
    case 0x7B: writeUnicode(0x30B7); return 1;  //シ
    case 0x7C: writeUnicode(0x30B9); return 1;  //ス
    case 0x7D: writeUnicode(0x30BB); return 1;  //セ
    case 0x7E: writeUnicode(0x30BD); return 1;  //ソ
    case 0x7F: writeUnicode(0x30BF); return 1;  //タ
    case 0x80: writeUnicode(0x30C1); return 1;  //チ
    case 0x81: writeUnicode(0x30C4); return 1;  //ツ
    case 0x82: writeUnicode(0x30C6); return 1;  //テ
    case 0x83: writeUnicode(0x30C8); return 1;  //ト
    case 0x84: writeUnicode(0x30CA); return 1;  //ナ
    case 0x85: writeUnicode(0x30CB); return 1;  //ニ
    case 0x86: writeUnicode(0x30CC); return 1;  //ヌ
    case 0x87: writeUnicode(0x30CD); return 1;  //ネ
    case 0x88: writeUnicode(0x30CE); return 1;  //ノ
    case 0x89: writeUnicode(0x30CF); return 1;  //ハ
    case 0x8A: writeUnicode(0x30D2); return 1;  //ヒ
    case 0x8B: writeUnicode(0x30D5); return 1;  //フ
    case 0x8C: writeUnicode(0x30D8); return 1;  //ヘ
    case 0x8D: writeUnicode(0x30DB); return 1;  //ホ
    case 0x8E: writeUnicode(0x30DE); return 1;  //マ
    case 0x8F: writeUnicode(0x30DF); return 1;  //ミ
    case 0x90: writeUnicode(0x30E0); return 1;  //ム
    case 0x91: writeUnicode(0x30E1); return 1;  //メ
    case 0x92: writeUnicode(0x30E2); return 1;  //モ
    case 0x93: writeUnicode(0x30E4); return 1;  //ヤ
    case 0x94: writeUnicode(0x30E6); return 1;  //ユ
    case 0x95: writeUnicode(0x30E8); return 1;  //ヨ
    case 0x96: writeUnicode(0x30E9); return 1;  //ラ
    case 0x97: writeUnicode(0x30EA); return 1;  //リ
    case 0x98: writeUnicode(0x30EB); return 1;  //ル
    case 0x99: writeUnicode(0x30EC); return 1;  //レ
    case 0x9A: writeUnicode(0x30ED); return 1;  //ロ
    case 0x9B: writeUnicode(0x30EF); return 1;  //ワ
    case 0x9C: writeUnicode(0x30F2); return 1;  //ヲ
    case 0x9D: writeUnicode(0x30F3); return 1;  //ン
    case 0x9E: writeUnicode(0x25B6); return 1;  //▶
    case 0x9F: writeUnicode(0x25BC); return 1;  //▼
    case 0xA0: writeUnicode(0x002D); return 1;  //-
    case 0xA1: writeUnicode(0x002B); return 1;  //+
    case 0xA2: writeUnicode(0x002F); return 1;  // /
    case 0xA3: writeUnicode(0x00D7); return 1; //×
    case 0xA4: writeUnicode(0x25B2); return 1; //▲
    case 0xA5: writeUnicode(0x0020); return 1; //Leer
    case 0xA6: writeUnicode(0x0020); return 1; //Leer
    case 0xA7: writeUnicode(0x0020); return 1; //Leer
    case 0xA8: writePlainText("<boxTopLeft>"); return 1; //<boxTopLeft>
    case 0xA9: writePlainText("<boxTop>"); return 1; //<boxTop>
    case 0xAA: writePlainText("<boxTopRight>"); return 1; //<boxTopRight>
    case 0xAB: writePlainText("<boxBottomLeft>"); return 1; //<boxBottomLeft>
    case 0xAC: writePlainText("<boxBottom>"); return 1; //<boxBottomOr-?>
    case 0xAD: writePlainText("<boxBottomRight>"); return 1; //<boxBottomRight>
    case 0xAE: writePlainText("<boxLeft>"); return 1; //<boxLeft>
    case 0xAF: writePlainText("<boxRight>"); return 1; //<boxRight>
    case 0xB0: writeUnicode(0x304C); return 1;   //が
    case 0xB1: writeUnicode(0x304E); return 1;   //ぎ
    case 0xB2: writeUnicode(0x3050); return 1;   //ぐ
    case 0xB3: writeUnicode(0x3052); return 1;   //げ
    case 0xB4: writeUnicode(0x3054); return 1;   //ご
    case 0xB5: writeUnicode(0x3056); return 1;   //ざ
    case 0xB6: writeUnicode(0x3058); return 1;   //じ
    case 0xB7: writeUnicode(0x305A); return 1;   //ず
    case 0xB8: writeUnicode(0x305C); return 1;   //ぜ
    case 0xB9: writeUnicode(0x305E); return 1;   //ぞ
    case 0xBA: writeUnicode(0x3060); return 1;   //だ
    case 0xBB: writeUnicode(0x3062); return 1;   //ぢ
    case 0xBC: writeUnicode(0x3065); return 1;   //づ
    case 0xBD: writeUnicode(0x3067); return 1;   //で
    case 0xBE: writeUnicode(0x3069); return 1;   //ど
    case 0xBF: writeUnicode(0x3070); return 1;   //ば
    case 0xC0: writeUnicode(0x3073); return 1;   //び
    case 0xC1: writeUnicode(0x3076); return 1;   //ぶ
    case 0xC2: writeUnicode(0x3079); return 1;   //べ
    case 0xC3: writeUnicode(0x307C); return 1;   //ぼ
    case 0xC4: writeUnicode(0x30AC); return 1;   //ガ
    case 0xC5: writeUnicode(0x30AE); return 1;   //ギ
    case 0xC6: writeUnicode(0x30B0); return 1;   //グ
    case 0xC7: writeUnicode(0x30B2); return 1;   //ゲ
    case 0xC8: writeUnicode(0x30B4); return 1;   //ゴ
    case 0xC9: writeUnicode(0x30B6); return 1;   //ザ
    case 0xCA: writeUnicode(0x30B8); return 1;   //ジ
    case 0xCB: writeUnicode(0x30BA); return 1;   //ズ
    case 0xCC: writeUnicode(0x30BC); return 1;   //ゼ
    case 0xCD: writeUnicode(0x30BE); return 1;   //ゾ
    case 0xCE: writeUnicode(0x30C0); return 1;   //ダ
    case 0xCF: writeUnicode(0x30C2); return 1;   //ヂ
    case 0xD0: writeUnicode(0x30C5); return 1;   //ヅ
    case 0xD1: writeUnicode(0x30C7); return 1;   //デ
    case 0xD2: writeUnicode(0x30C9); return 1;   //ド
    case 0xD3: writeUnicode(0x30D0); return 1;   //バ
    case 0xD4: writeUnicode(0x30D3); return 1;   //ビ
    case 0xD5: writeUnicode(0x30D6); return 1;   //ブ
    case 0xD6: writeUnicode(0x30D9); return 1;   //ベ
    case 0xD7: writeUnicode(0x30DC); return 1;   //ボ
    case 0xD8: writeUnicode(0x30F4); return 1;   //ヴ
    case 0xD9: writeUnicode(0x3071); return 1;   //ぱ
    case 0xDA: writeUnicode(0x3074); return 1;   //ぴ
    case 0xDB: writeUnicode(0x3077); return 1;   //ぷ
    case 0xDC: writeUnicode(0x307A); return 1;   //ぺ
    case 0xDD: writeUnicode(0x307D); return 1;   //ぽ
    case 0xDE: writeUnicode(0x30D1); return 1;   //パ
    case 0xDF: writeUnicode(0x30D4); return 1;   //ピ
    case 0xE0: writeUnicode(0x30D7); return 1;   //プ
    case 0xE1: writeUnicode(0x30DA); return 1;   //ペ
    case 0xE2: writeUnicode(0x30DD); return 1;   //ポ
    case 0xE3: writeUnicode(0x000A); return 1;  //line break
    case 0xE4: writePlainText("<press>"); return 1;
    case 0xE5: writePlainText("<closeTextBox>"); return 1;
    case 0xE6: writePlainText("shortWait", 1); return 2;
    case 0xE7: writePlainText("<clr>"); return 1;
    case 0xE8: writePlainText("playSnd", 1); return 2;
    case 0xE9: writePlainText("script?,0xe9", 6); return 7;
    //case 0xEA: writePlainText("<unkown,0xea>"); return 1;
    case 0xEA: writePlainText("<doesntExist,0xea>"); return 1;
    case 0xEB: writePlainText("yesNoBox", 4); return 5;
    case 0xEC: writePlainText("<script?,0xec>"); return 1;
    //case 0xED: writePlainText("<unkown,0xed>"); return 1;
    case 0xED: writePlainText("<doesntExist,0xed>"); return 1;
    case 0xEE: writePlainText("script?,0xee", 3); return 4;
    case 0xEF: writePlainText("<inserts item?>"); return 1;
    case 0xF0: writePlainText("script?,0xf0", 1); return 2;
    case 0xF1: writePlainText("script?,0xf1", 1); return 2;
    case 0xF2: writePlainText("jump,0xf2", 6); return 7;
    case 0xF3: writePlainText("<script?,0xf3>"); return 1;
    case 0xF4: writePlainText("jump,0xf4", 5); return 6;
    case 0xF5: writePlainText("jump,0xf5", 7); return 8;
    case 0xF6: writePlainText("<currentPassword>"); return 1;
    case 0xF7: writePlainText("<inserts number?>"); return 1;
    case 0xF8: writePlainText("jump,0xf8", 4); return 5;
    case 0xF9: writePlainText("jump,0xf9", 5); return 6;
    case 0xFA: writePlainText("jump,0xfa", 5); return 6;
    case 0xFB: writePlainText("script?,0xfb", 1); return 2;
    case 0xFC: writePlainText("<doesntExist,0xfc>"); return 1;
    //case 0xFD: writePlainText("<unkown,0xfd>"); return 1;
    //case 0xFE: writePlainText("<unkown,0xfe>"); return 1;
    case 0xFD: writePlainText("<doesntExist,0xfd>"); return 1;
    case 0xFE: writePlainText("<doesntExist,0xfe>"); return 1;
    case 0xFF: writePlainText("<doesntExist,0xff>"); return 1;
    default: writePlainText("<warning default case!>"); return 1;
    }
}

void QGBTextbank::dumpByteArray(QByteArray *byteArray)
{
    quint16 i = 0;
    QString line = "0x" + QString("%1: ").arg(i, 4, 16, QChar('0')).toUpper();
    while (i < byteArray->size())
    {
        line += QString("%1").arg((quint8)byteArray->at(i), 2, 16, QChar('0')).toUpper();
        i++;
        if (i % 16 == 0)
        {
            qDebug() << line;
            line = "0x" + QString("%1: ").arg(i, 4, 16, QChar('0')).toUpper();
        }
        else if (i % 4 == 0)
        {
            line += " ";
        }
    }
    if (i % 16 != 0)
        qDebug() << line;
}
