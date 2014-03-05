#include "QGBArchive.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>

QGBArchive::QGBArchive(QString romFile, quint64 offset, QString uncompressedFile, QGBArchiveMode mode, QGBDeCompress::QGBBlockSize blockSize, quint64 maxSize)
{
    if (mode == QAMExtract)
        extract(romFile, offset, uncompressedFile);
    else if (mode == QAMInsert)
        insert(romFile, offset, uncompressedFile, blockSize, maxSize);
    else
    {
        qDebug() << "something went wrong";
        return;
    }
}

bool QGBArchive::extract(QString romFile, quint64 offset, QString uncompressedFile)
{
    QFile *rom = new QFile(romFile);
    QFile uncomp(uncompressedFile);

    rom->open(QIODevice::ReadOnly);

    quint16 size;
    QGBDeCompress *decomp = new QGBDeCompress();
    QByteArray *decompressed = decomp->decompress(rom, offset, size);
    delete decomp;

    rom->close();
    delete rom;

    if (decompressed != NULL)
    {
        uncomp.open(QIODevice::WriteOnly);
        uncomp.seek(0);
        QDataStream out(&uncomp);

        for (int i = 0; i < decompressed->size(); i++)
            out << (quint8)decompressed->at(i);

        delete decompressed;
        uncomp.close();
        return true;
    }

    return false;
}

bool QGBArchive::insert(QString romFile, quint64 offset, QString uncompressedFile, QGBDeCompress::QGBBlockSize blockSize, quint64 maxSize)
{
    QFile rom(romFile);
    QFile *uncomp = new QFile(uncompressedFile);

    uncomp->open(QIODevice::ReadOnly);
    uncomp->seek(0);

    QGBDeCompress *comp = new QGBDeCompress();
    QByteArray *compressed = comp->compress(uncomp, blockSize);
    delete comp;

    uncomp->close();
    delete uncomp;

    if (compressed != NULL)
    {
        if (compressed->size() > maxSize)
        {
            qDebug() << "WARNING: compressed archive too large!";
            qDebug() << "nothing written";
            qDebug() << QString("max size %1, compressed size %2").arg(maxSize).arg(compressed->size());
            delete compressed;
            return false;
        }

        qDebug() << QString("max size %1, compressed size %2").arg(maxSize).arg(compressed->size());

        rom.open(QIODevice::ReadWrite);
        rom.seek(offset);
        QDataStream out(&rom);

        for (int i = 0; i < compressed->size(); i++)
            out << (quint8)compressed->at(i);

        delete compressed;
        rom.close();
        return true;
    }

    return false;
}

