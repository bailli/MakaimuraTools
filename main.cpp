#include <QtCore/QTextStream>

#include "QGBTextbank.h"
#include "QGBArchive.h"

#define RELVL

#define OFFSET_TILESET 0x8006
#define SIZE_TILESET 1305
#define OFFSET_CHARSCREEN 0x9826
#define SIZE_CHARSCREEN 236
#define OFFSET_LVLUSEMAGIC 0x97F3
#define SIZE_LVLUSEMAGIC 51
#define OFFSET_NEWPWD 0x28006
#define SIZE_NEWPWD 63
#define OFFSET_MAGIC 0x9941
#define SIZE_MAGIC 73 //incorrect?
#define OFFSET_TOOLS 0x20006
#define SIZE_TOOLS 175 //incorrect?
#define OFFSET_BREAGER 0x36CFA
#define SIZE_BREAGER 34
#define OFFSET_NAGUS 0x36D1C
#define SIZE_NAGUS 26
#define OFFSET_FIREBRAND 0x36D36
#define SIZE_FIREBRAND 30
#define OFFSET_SANDFROG 0x36D54
#define SIZE_SANDFROG 31
#define OFFSET_TWINS 0x36D73
#define SIZE_TWINS 33
#define OFFSET_GOZA 0x36CDF
#define SIZE_GOZA 27
#define OFFSET_DOPPEL 0x36CBC
#define SIZE_DOPPEL 35
#define OFFSET_BALLOON 0x36C9E
#define SIZE_BALLOON 30
#define OFFSET_ENDTILESET 0x2BDFE
#define SIZE_ENDTILESET 215
#define OFFSET_LEGEND 0x2A2D7
#define LEGEND_SIZE 156
#define OFFSET_ENDING 0x2A4FC
#define ENDING_SIZE 269
#define OFFSET_TITLETILESET 0x2C006
#define TITLETILESET_SIZE 120
#define OFFSET_TITLEMAP 0x107F1
#define TITLEMAP_SIZE 320
#define OFFSET_CAPCOMTILESET 0x2BED5
#define CCTILESET_SIZE 144
#define OFFSET_CCMAP 0x2BF65
#define CCMAP_SIZE 47

int main()
{
    //QCoreApplication app(argc, argv); //renamed the a to app
    QTextStream qout(stdout); //I connect the stout to my qout textstream

    qout << "Makaimura Gaiden Toolset v0.5" << endl;
    qout << "(c) 2012 by bailli" << endl << endl;

#ifdef MAP
    qout << "decompressing map.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_MAP, "/home/bailli/gb/map.map");
    delete archive;
#endif

#ifdef RECCMAP
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/betaV7.gb", OFFSET_CCMAP, "/home/bailli/gb/ccmap_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS64bytes, CCMAP_SIZE);
    delete archive;
#endif

#ifdef CCMAP
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_CCMAP, "/home/bailli/gb/ccmap.map");
    delete archive;
#endif

#ifdef RETITLEMAP
    qout << "compressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/rc2.gb", OFFSET_TITLEMAP, "/home/bailli/gb/title_bailli.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, TITLEMAP_SIZE);
    delete archive;
#endif

#ifdef TITLEMAP
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_TITLEMAP, "/home/bailli/gb/title.map");
    delete archive;
#endif

#ifdef RETITLETILES
    qout << "compressing tileset.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/rc2.gb", 0x31C00, "/home/bailli/gb/titletiles_eng.gb", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 2900);
    delete archive;
#endif

#ifdef TITLETILES
    qout << "decompressing tileset.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_TITLETILESET, "/home/bailli/gb/titletiles.gb");
    delete archive;
#endif

#ifdef CCTILES
    qout << "decompressing tileset.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_CAPCOMTILESET, "/home/bailli/gb/cctiles.gb");
    delete archive;
#endif

#ifdef RECCTILES
    qout << "compressing tileset.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/betaV7.gb", OFFSET_CAPCOMTILESET, "/home/bailli/gb/thankyou.gb", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, CCTILESET_SIZE);
    delete archive;
#endif

#ifdef ENDTILES
    qout << "decompressing tileset.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_ENDTILESET, "/home/bailli/gb/endtiles.gb");
    delete archive;
#endif

#ifdef REENDTILES
    qout << "compressing tileset.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x23980, "/home/bailli/gb/endtiles_latin.gb", QGBArchive::QAMInsert, QGBDeCompress::QBS64bytes, 256);
    delete archive;
#endif

#ifdef BALLOON
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_BALLOON, "/home/bailli/gb/balloon.map");
    delete archive;
#endif

#ifdef REBOSS
    qout << "compressing boss screen.." << endl;
    QGBArchive *archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37CF0, "/home/bailli/gb/nagus_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37D20, "/home/bailli/gb/balloon_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37D50, "/home/bailli/gb/sandfrog_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37D80, "/home/bailli/gb/twins_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37DB0, "/home/bailli/gb/doppel_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37DE0, "/home/bailli/gb/goza_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37E10, "/home/bailli/gb/breager_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
    archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x37E40, "/home/bailli/gb/firebrand_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, 48);
    delete archive;
#endif

#ifdef DOPPEL
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_DOPPEL, "/home/bailli/gb/doppel.map");
    delete archive;
#endif

#ifdef GOZA
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_GOZA, "/home/bailli/gb/goza.map");
    delete archive;
#endif

#ifdef TWINS
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_TWINS, "/home/bailli/gb/twins.map");
    delete archive;
#endif

#ifdef SANDFROG
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_SANDFROG, "/home/bailli/gb/sandfrog.map");
    delete archive;
#endif

#ifdef FIREBRAND
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_FIREBRAND, "/home/bailli/gb/firebrand.map");
    delete archive;
#endif

#ifdef NAGUS
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_NAGUS, "/home/bailli/gb/nagus.map");
    delete archive;
#endif

#ifdef BREAGER
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_BREAGER, "/home/bailli/gb/breager.map");
    delete archive;
#endif

#ifdef UNCOMP
    qout << "uncompressing font tile set.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_TILESET, "/home/bailli/gb/tiledecomp.gb");
    delete archive;
#endif

#ifdef LEGEND
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_LEGEND, "/home/bailli/gb/legend.map");
    delete archive;
#endif

#ifdef RELEGEND
    qout << "compressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/betaV7.gb", OFFSET_LEGEND-4, "/home/bailli/gb/legend_comp.map", QGBArchive::QAMInsert, QGBDeCompress::QBS64bytes, 160);
    delete archive;
#endif


#ifdef ENDING
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/betaV4.gb", OFFSET_ENDING, "/home/bailli/gb/ending_eng.map");
    delete archive;
#endif

#ifdef REENDING
    qout << "compressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/betaV6.gb", 0x2A516, "/home/bailli/gb/ending_comp_aligned.map", QGBArchive::QAMInsert, QGBDeCompress::QBS64bytes, 243);
    delete archive;
#endif

#ifdef LVLUSEMAGIC
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/spanish/GQ2S.gb", OFFSET_LVLUSEMAGIC, "/home/bailli/gb/spanish/lvlusemagic_esp.map");
    delete archive;
#endif

#ifdef RELVL
    qout << "compressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/spanish/GQ2S_.gb", OFFSET_LVLUSEMAGIC, "/home/bailli/gb/spanish/lvlusemagic_esp.map", QGBArchive::QAMInsert, QGBDeCompress::QBS16bytes, SIZE_LVLUSEMAGIC);
    delete archive;
#endif

#ifdef NEWPWD
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_NEWPWD, "/home/bailli/gb/newpwd.map");
    delete archive;
#endif

#ifdef TOOLS
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_TOOLS, "/home/bailli/gb/tools.map");
    delete archive;
#endif

#ifdef RETOOLS
    qout << "compressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/betaV2_candle.gb", 0x23820, "/home/bailli/gb/tools_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS64bytes, 208);
    delete archive;
#endif


#ifdef MAGIC
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_MAGIC, "/home/bailli/gb/magic.map");
    delete archive;
#endif

#ifdef REMAGIC
    qout << "compressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/testing_dte_menu.gb", 0x238F0, "/home/bailli/gb/magic_large_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS16bytes, 96);
    delete archive;
#endif

#ifdef RENEWPWD
    qout << "compressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/testing_dte_menu.gb", OFFSET_NEWPWD, "/home/bailli/gb/newpwd_eng.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, SIZE_NEWPWD);
    delete archive;
#endif

#ifdef RECOMP
    qout << "compressing and inserting font tile set.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/spanish/GQ2S.gb", OFFSET_TILESET, "/home/bailli/gb/spanish/fonttileset-esp.gb", QGBArchive::QAMInsert, QGBDeCompress::QBS64bytes, SIZE_TILESET);
    delete archive;
#endif

#ifdef SCREEN
    qout << "decompressing screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/untouched.gb", OFFSET_CHARSCREEN, "/home/bailli/gb/charscreen.map");
    delete archive;
#endif

#ifdef RESCREEN
    qout << "compressing and inserting screen.." << endl;
    QGBArchive *archive = new QGBArchive("/home/bailli/gb/spanish/GQ2S_.gb", OFFSET_CHARSCREEN, "/home/bailli/gb/spanish/charscreen_esp.map", QGBArchive::QAMInsert, QGBDeCompress::QBS32bytes, SIZE_CHARSCREEN);
    delete archive;
#endif


#ifdef INSERT
    qout << "resinert full text bank.." << endl;
    QGBTextbank *txtdump = new QGBTextbank("/home/bailli/gb/final.gb", "/home/bailli/gb/final.txt");
    txtdump->insertText();
    delete txtdump;
#endif

#ifdef DUMP
    qout << "dump full text bank.." << endl;
    QGBTextbank *txtdump = new QGBTextbank("/home/bailli/gb/untouched.gb", "/home/bailli/gb/testdump.txt");
    txtdump->dumpText();
    delete txtdump;
#endif

    return 0;
}
