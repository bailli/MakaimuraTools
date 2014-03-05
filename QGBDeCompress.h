#ifndef QGBDECOMPRESS_H
#define QGBDECOMPRESS_H

#include <QtCore/QFile>
#include <QtCore/QByteArray>

class QGBDeCompress
{

public:
    enum QGBBlockSize { QBS16bytes = 0x10, QBS32bytes = 0x20, QBS64bytes = 0x40 };
    enum QGBCompressionType { QCTZero = 0x00, QCTBitmaskOnly = 0x01, QCTXor64bytes = 0x02, QCTXor32bytes = 0x03 };

    QGBDeCompress();
    ~QGBDeCompress();
    QByteArray *decompress(QFile *file, quint64 offset, quint16 &fullSize);
    QByteArray *compress(QFile *file, QGBBlockSize blockSize, quint64 offset = 0, quint64 length = 0);

private:
    QByteArray *readAndUncompressBlock(QFile *inFile, QGBBlockSize blockSize, QGBCompressionType compType);
    QByteArray *xorCompress(QByteArray *uncompressed, QGBCompressionType compType);
    QByteArray *compressToBitmaskBlock(QByteArray *uncompressed, QGBBlockSize blockSize);
    void dumpByteArray(QByteArray *byteArray);
/*    QByteArray *compressByteField();
    void xorUncompressByteField();
    void xorCompressByteField();
    void dumpByteArray(QByteArray *byteArray);
    void writeByteField(QByteArray *byteArray, QFile *outFile);*/
};

#endif // QGBDECOMPRESS_H


//after data block there are 11+5 bytes for compression type selection
//there are 4 types => 2 bits per block
//after 3 bytes there are five 0x00 bytes because of the gap in vram inbetween the tileset
//bits 01: only "readByteField"
//bits 10: readByteField+xorByteField
//bits 11: as 10 with changed colours (is not used in font tile set)
//bits 00: just some memory management (as far as I see it)

// A6 0110 1010  --> 0x03
// AA 1010 1010
// A6 1010 0110  --> 0x09
//
// 00 0000 0000
// 00 0000 0000
// 00 0000 0000
// 00 0000 0000
// 00 0000 0000
//
// A5 1010 0101  --> 0x0C 0x0D
// AA 1010 1010
// AA 1010 1010
// AA 1010 1010
// A9 1010 1001  --> 0x1C
//
// AA 1010 1010
// 9A 1001 1010  --> 0x26
// AA 1010 1010
