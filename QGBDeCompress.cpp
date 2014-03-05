#include "QGBDeCompress.h"

#include <QtCore/QDataStream>
#include <QtCore/QDebug>

QGBDeCompress::QGBDeCompress()
{

}

QGBDeCompress::~QGBDeCompress()
{

}

QByteArray *QGBDeCompress::decompress(QFile *file, quint64 offset, quint16 &fullSize)
{
    if (!(file->openMode() & QIODevice::ReadOnly))
    {
        qDebug() << "QFile not opened!";
        return NULL;
    }

    file->seek(offset);
    QDataStream in(file);
    in.setByteOrder(QDataStream::LittleEndian);

    QGBBlockSize blockSize;
    quint8 byte;

    in >> byte;
    if (byte == 0x11)
        blockSize = QBS32bytes;
    else if (byte == 0x12)
        blockSize = QBS64bytes;
    else if (byte == 0x10)
        blockSize = QBS16bytes;
    else
    {
        //unkown type!
        qDebug() << "unkown archive type! starting byte" << byte;
        return NULL;
    }

    quint16 blockCount;
    in >> blockCount;

    quint16 archiveSize;
    in >> archiveSize;

    QByteArray compressionFlags;
    file->seek(offset + archiveSize);

    quint8 blocksPerFlag;
    if (blockSize == QBS16bytes)
        blocksPerFlag = 8;
    else
        blocksPerFlag = 4;
    quint8 flagByteCount = blockCount / blocksPerFlag;
    if ((blockCount % blocksPerFlag) != 0)
        flagByteCount++;

    for (int i = 0; i < flagByteCount; i++)
    {
        in >> byte;
        compressionFlags.append(byte);
    }

    file->seek(offset + 5);

    fullSize = archiveSize + flagByteCount;

    quint8 flagByteCounter = 0;
    quint8 rollCounter = blocksPerFlag;
    quint8 currentFlagByte = (quint8)compressionFlags.at(flagByteCounter);

    QGBCompressionType compType;
    QByteArray *currentBlock;
    QByteArray *decompressedArchive = new QByteArray();

    for (int i = 0; i < blockCount; i++)
    {
        //get current compression type
        if (blockSize == QBS16bytes)
        {
            byte = currentFlagByte & 0x01;
            if (byte == 0x01)
                compType = QCTBitmaskOnly;
            else if (byte == 0x00)
                compType = QCTXor64bytes;
        }
        else
        {
            byte = currentFlagByte & 0x03;
            if (byte == 0x00)
                compType = QCTZero;
            else if (byte == 0x01)
                compType = QCTBitmaskOnly;
            else if (byte == 0x02)
                compType = QCTXor64bytes;
            else if (byte == 0x03)
                compType = QCTXor32bytes;
            else
            {
                //unkown compression type!
                qDebug() << "unkown compression type found:" << byte;
                return NULL;
            }
        }

        //advance flag
        currentFlagByte >>= (8/blocksPerFlag);
        rollCounter--;
        if (!rollCounter)
        {
            rollCounter = blocksPerFlag;
            flagByteCounter++;
            currentFlagByte = (quint8)compressionFlags.at(flagByteCounter);
        }

        currentBlock = readAndUncompressBlock(file, blockSize, compType);
        decompressedArchive->append(*currentBlock);
        delete currentBlock;
    }

    file->seek(offset + archiveSize + flagByteCount);

    return decompressedArchive;
}

QByteArray *QGBDeCompress::readAndUncompressBlock(QFile *inFile, QGBBlockSize blockSize, QGBCompressionType compType)
{
    QDataStream in(inFile);
    QByteArray bitMask;
    QByteArray *decompressedBlock = new QByteArray();
    quint8 byte;
    quint8 andValue;

    //read in bitmask bytes
    quint8 bitmaskByteCount;
    if (blockSize == QBS32bytes)
    {
        bitmaskByteCount = 4;
        decompressedBlock->resize(32);
    }
    else if (blockSize == QBS64bytes)
    {
        bitmaskByteCount = 8;
        decompressedBlock->resize(64);
    }
    else if (blockSize == QBS16bytes)
    {
        bitmaskByteCount = 2;
        decompressedBlock->resize(16);
    }
    else
    {
        //something is wrong...
        qDebug() << "something is wrong...";
        return NULL;
    }

    if (compType == QCTZero)
    {
        decompressedBlock->fill(0x00);
        return decompressedBlock;
    }

    for (int i = 0; i < bitmaskByteCount; i++)
    {
        in >> byte;
        bitMask.append(byte);
    }

    //unpack corresponding to bitmask
    for (int i = 0; i < bitmaskByteCount; i++)
    {
        andValue = 0x01;
        for (int j = 0; j < 8; j++)
        {
            if (bitMask.at(i) & andValue)
            {
                in >> byte;
                (*decompressedBlock)[i*8 + j] = byte;
            }
             else
                (*decompressedBlock)[i*8 + j] = 0x00;
            andValue <<= 1;
        }
    }

    //xor if necessary
    if (compType == QCTXor32bytes)
    {
        for (int i = 1; i < decompressedBlock->size(); i++)
            (*decompressedBlock)[i] = (*decompressedBlock)[i-1] ^ (*decompressedBlock)[i];
    }
    else if (compType == QCTXor64bytes)
    {
        quint8 k;
        for (int i = 0; i < 7; i++)
            for (int j = 0; j < decompressedBlock->size(); j += 0x10)
            {
                k = 2*i + j;
                (*decompressedBlock)[k+2] = (*decompressedBlock)[k+0] ^ (*decompressedBlock)[k+2];
                (*decompressedBlock)[k+3] = (*decompressedBlock)[k+1] ^ (*decompressedBlock)[k+3];
            }
    }

    return decompressedBlock;
}


QByteArray *QGBDeCompress::compress(QFile *file, QGBBlockSize blockSize, quint64 offset, quint64 length)
{
    if (!(file->openMode() & QIODevice::ReadOnly))
    {
        qDebug() << "QFile not opened!";
        return NULL;
    }

    if (length == 0)
        length = file->size();

    file->seek(offset);

    if ((length % blockSize) != 0)
    {
        //length must be multiple of 16 or 32!
        qDebug() << "length must be multiple of blockSize!";
        return NULL;
    }

    quint16 blockCount = length / blockSize;
    QByteArray compressionFlags;
    quint8 currentFlagByte = 0x00;
    quint8 rollCounter = 0;
    quint8 blocksPerFlag;
    QByteArray *uncompressedBlock;
    QByteArray *xorBlock;
    QByteArray *bitMaskBlock;
    QByteArray *bitMaskXorBlock;
    quint8 byte;
    QDataStream in(file);

    QByteArray *compressedArchive = new QByteArray();

    if (blockSize == QBS16bytes)
        blocksPerFlag = 8;
    else
        blocksPerFlag = 4;

    //fill in header
    if (blockSize == QBS16bytes)
        compressedArchive->append((quint8)0x10);
    else if (blockSize == QBS64bytes)
        compressedArchive->append((quint8)0x12);
    else if (blockSize == QBS32bytes)
        compressedArchive->append((quint8)0x11);
    else
    {
        //Something went wrong
        delete compressedArchive;
        return NULL;
    }
    //block count
    compressedArchive->append((quint8)(blockCount & 0xFF));
    compressedArchive->append((quint8)(blockCount >> 8));
    //place holder for size
    compressedArchive->append(QChar(0x00));
    compressedArchive->append(QChar(0x00));

    for (int i = 0; i < blockCount; i++)
    {
        uncompressedBlock = new QByteArray();

        //read block
        for (int j = 0; j < blockSize; j++)
        {
            in >> byte;
            uncompressedBlock->append(byte);
        }

        //check for all 0x00!
        bool allZero = true;
        for (int j = 0; j < uncompressedBlock->size(); j++)
            if (uncompressedBlock->at(j) != 0x00)
            {
                allZero = false;
                break;
            }

        if (!allZero)
        {
            //compare sizes for with and without xor
            if ((blockSize == QBS64bytes) || (blockSize == QBS16bytes))
                xorBlock = xorCompress(uncompressedBlock, QCTXor64bytes);
            else if (blockSize == QBS32bytes)
                xorBlock = xorCompress(uncompressedBlock, QCTXor32bytes);
            else
            {
                //something went wrong
                delete uncompressedBlock;
                delete compressedArchive;
                return NULL;
            }

            bitMaskBlock = compressToBitmaskBlock(uncompressedBlock, blockSize);
            bitMaskXorBlock = compressToBitmaskBlock(xorBlock, blockSize);

            //append smaller one
            if (bitMaskBlock->size() <= bitMaskXorBlock->size())
            {
                compressedArchive->append(*bitMaskBlock);
                if (blockSize == QBS16bytes)
                    currentFlagByte |= 0x80;
                else
                    currentFlagByte |= 0x40;
                qDebug() << QString("block 0x%1 is not xor compressed").arg(i, 2, 16, QChar('0'));
            }
            else
            {
                compressedArchive->append(*bitMaskXorBlock);
                if (blockSize == QBS64bytes)
                    currentFlagByte |= 0x80;
                else if (blockSize == QBS32bytes)
                    currentFlagByte |= 0xC0;
                else if (blockSize == QBS16bytes)
                    currentFlagByte |= 0x00;
                else
                {
                    //something went wrong
                    return NULL;
                }
                qDebug() << QString("block 0x%1 is xor compressed").arg(i, 2, 16, QChar('0'));
            }

            delete bitMaskBlock;
            delete bitMaskXorBlock;
        }
        else
        {
            qDebug() << QString("block 0x%1 is all zero").arg (i, 2, 16, QChar('0'));
        }

        delete uncompressedBlock;

        //roll flag byte and check if "full"
        rollCounter++;

        if (rollCounter == blocksPerFlag)
        {
            rollCounter = 0;
            compressionFlags.append(currentFlagByte);
            currentFlagByte = 0x00;
        }

        currentFlagByte >>= (8 / blocksPerFlag);
    }

    //check last flag byte!
    if (rollCounter != 0)
    {
        currentFlagByte >>= ((8 / blocksPerFlag) * (blocksPerFlag - 1 - rollCounter));
        compressionFlags.append(currentFlagByte);
    }

    //append compression flag bytes
    quint16 fullSize = compressedArchive->size();
    compressedArchive->append(compressionFlags);

    //correct size
    (*compressedArchive)[3] = (quint8)(fullSize & 0xFF);
    (*compressedArchive)[4] =  (quint8)(fullSize >> 8);

    return compressedArchive;
}

QByteArray *QGBDeCompress::compressToBitmaskBlock(QByteArray *uncompressed, QGBBlockSize blockSize)
{
    QByteArray *compressedBlock = new QByteArray();
    compressedBlock->resize(uncompressed->size() + 8);

    quint8 bitmaskPos = 0;
    quint8 dataPos;
    quint8 bitMaskByte = 0x00;
    quint8 currentBit = 0x01;

    if (blockSize == QBS32bytes)
        dataPos = 4;
    else if (blockSize == QBS64bytes)
        dataPos = 8;
    else if (blockSize == QBS16bytes)
        dataPos = 2;
    else
    {
        //something went wrong
        return NULL;
    }

    for (int i = 0; i < uncompressed->size(); i++)
    {
        if ((quint8)(uncompressed->at(i)) != 0x00)
        {
            bitMaskByte |= currentBit;
            (*compressedBlock)[dataPos] = uncompressed->at(i);
            dataPos++;
        }

        if (currentBit == 0x80)
        {
            (*compressedBlock)[bitmaskPos] = bitMaskByte;
            bitMaskByte = 0x00;
            currentBit = 0x01;
            bitmaskPos++;
        }
        else
            currentBit <<= 1;
    }

    compressedBlock->resize(dataPos);

    return compressedBlock;
}

QByteArray *QGBDeCompress::xorCompress(QByteArray *uncompressed, QGBCompressionType compType)
{
    QByteArray *xorCompressed = new QByteArray();
    xorCompressed->append(*uncompressed);

    if (compType == QCTXor64bytes)
    {
        quint8 k;
        for (int i = 0; i < xorCompressed->size(); i += 0x10)
            for (int j = 0x0F; j > 0x01; j -= 2)
            {
                k = i + j;
                (*xorCompressed)[k] = (*xorCompressed)[k] ^ (*xorCompressed)[k-2];
                (*xorCompressed)[k-1] = (*xorCompressed)[k-1] ^ (*xorCompressed)[k-3];
            }
    }
    else if (compType == QCTXor32bytes)
    {
        for (int i = xorCompressed->size()-1; i > 0x00; i--)
                (*xorCompressed)[i] = (*xorCompressed)[i] ^ (*xorCompressed)[i-1];
    }
    else
    {
        //something went wrong...
        return NULL;
    }

    return xorCompressed;
}

void QGBDeCompress::dumpByteArray(QByteArray *byteArray)
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
