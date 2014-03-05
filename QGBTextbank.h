#ifndef QGBTEXTBANK_H
#define QGBTEXTBANK_H

#include <QtCore/QObject>
#include <QtCore/QFile>

#define MAX_STRINGS 161

struct QGByesNoBox
{
    quint32 offset;
    quint8 textBlock;
    quint8 yes;
    quint8 no;
};

struct QGBjump
{
    quint32 writeOffset;
    quint8 textBlock;
    quint8 firstTxtBlock;
    quint16 firstTxtBlockOffset;
    quint8 secondTxtBlock;
    quint16 secondTxtBlockOffset;
};

class QGBTextbank : public QObject
{
    Q_OBJECT

private:
    quint16 txtPointers[MAX_STRINGS];
    QGByesNoBox yesNoBoxes[32];
    QGBjump txtJump[256];
    quint8 yesNoBoxCounter;
    quint8 txtJumpCounter;
    quint32 pointerToOffset(int i);

    bool updatePointer(int i, quint32 newOffset);
    quint8 writeDecodedTile(quint8 tileNum);
    void writeUnicode(quint16 unicodeNum);
    void writePlainText(QString text);
    void writePlainText(QString text, quint8 additionalBytes);
    void readPointers();
    void dumpPointer(int i);
    void writeByteField(QByteArray *byteArray, QFile *outFile);
    void dumpByteArray(QByteArray *byteArray);
    bool getDTE(quint8 &currentTile, quint8 lastTile);
    quint8 unicodeToTile(quint16 unicode);
    quint8 nameToTile(QString str);
    QByteArray *readTextBlock(QFile *in, quint8 blockNum);

    QFile *romFile;
    QFile *txtFile;

public:
    explicit QGBTextbank(QString rom, QString dump);
    ~QGBTextbank();
    void dumpText();
    void insertText();


signals:
    
public slots:
    
};

#endif // QGBTEXTBANK_H
