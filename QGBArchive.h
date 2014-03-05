#ifndef QGBARCHIVE_H
#define QGBARCHIVE_H

#include "QGBDeCompress.h"

class QGBArchive
{
private:
    bool extract(QString romFile, quint64 offset, QString uncompressedFile);
    bool insert(QString romFile, quint64 offset, QString uncompressedFile, QGBDeCompress::QGBBlockSize blockSize, quint64 maxSize);
public:
    enum QGBArchiveMode {QAMExtract = 0, QAMInsert = 1 };
    QGBArchive(QString romFile, quint64 offset, QString uncompressedFile, QGBArchiveMode mode = QAMExtract, QGBDeCompress::QGBBlockSize blockSize = QGBDeCompress::QBS64bytes, quint64 maxSize = 0);

};

#endif // QGBARCHIVE_H
