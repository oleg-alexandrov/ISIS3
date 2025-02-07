#include <QTextStream>
#include <QUuid>

#include "CubeAttribute.h"
#include "FileName.h"

#include "Blob.h"
#include "Brick.h"
#include "Fixtures.h"
#include "Portal.h"
#include "LineManager.h"
#include "SpecialPixel.h"
#include "TestUtilities.h"
#include "ControlNet.h"

namespace Isis {


  void TgoCassisModuleKernels::SetUp() {
    QVector<QString> ckKernels = {QString("data/tgoCassis/mapProjectedReingested/em16_tgo_cassis_tel_20160407_20221231_s20220316_v01_0_sliced_-143410.xc"),
                                  QString("data/tgoCassis/mapProjectedReingested/em16_tgo_cassis_tel_20160407_20221231_s20220316_v01_1_sliced_-143410.xc"),
                                  QString("data/tgoCassis/mapProjectedReingested/em16_tgo_sc_ssm_20180501_20180601_s20180321_v01_0_sliced_-143000.xc"),
                                  QString("data/tgoCassis/mapProjectedReingested/em16_tgo_sc_ssm_20180501_20180601_s20180321_v01_1_sliced_-143000.xc"),
                                  QString("data/tgoCassis/em16_tgo_cassis_tel_20160407_20221231_s20220402_v01_0_sliced_-143410.xc"),
                                  QString("data/tgoCassis/em16_tgo_cassis_tel_20160407_20221231_s20220402_v01_1_sliced_-143410.xc"),
                                  QString("data/tgoCassis/em16_tgo_sc_spm_20161101_20170301_s20191109_v01_0_sliced_-143000.xc"),
                                  QString("data/tgoCassis/em16_tgo_sc_spm_20161101_20170301_s20191109_v01_1_sliced_-143000.xc"),
                                  QString("data/tgoCassis/singleFrameletProj/em16_tgo_cassis_tel_20160407_20221231_s20220402_v01_0_sliced_-143410.xc"),
                                  QString("data/tgoCassis/singleFrameletProj/em16_tgo_cassis_tel_20160407_20221231_s20220402_v01_1_sliced_-143410.xc"),
                                  QString("data/tgoCassis/singleFrameletProj/em16_tgo_sc_spm_20161101_20170301_s20191109_v01_0_sliced_-143000.xc"),
                                  QString("data/tgoCassis/singleFrameletProj/em16_tgo_sc_spm_20161101_20170301_s20191109_v01_1_sliced_-143000.xc")};
    QVector<QString> tempCkKernels;
    QVector<QString> spkKernels = {QString("data/tgoCassis/mapProjectedReingested/CAS-M01-2018-05-05T23.11.48.767-RED-01029-B1_0.xsp"),
                                   QString("data/tgoCassis/mapProjectedReingested/CAS-M01-2018-05-05T23.11.48.767-RED-01029-B1_1.xsp"),
                                   QString("data/tgoCassis/CAS-MCO-2016-11-26T22.50.27.381_0.xsp"),
                                   QString("data/tgoCassis/CAS-MCO-2016-11-26T22.50.27.381_1.xsp"),
                                   QString("data/tgoCassis/singleFrameletProj/CAS-MCO-2016-11-26T22.58.02.583_0.xsp"),
                                   QString("data/tgoCassis/singleFrameletProj/CAS-MCO-2016-11-26T22.58.02.583_1.xsp")};
    QVector<QString> tempSpkKernels;

    for (int i = 0; i < ckKernels.size(); i++) {
      QString kernelFile = ckKernels[i];
      QString kernelExtension = kernelFile.split('.').last();
      QString targetFile = kernelPrefix.path() + "/" + QString::number(i) + '.' + kernelExtension;
      QFile::copy(kernelFile, targetFile);
      tempCkKernels.append(targetFile);
    }

    for (int i = 0; i < spkKernels.size(); i++) {
      QString kernelFile = spkKernels[i];
      QString kernelExtension = kernelFile.split('.').last();
      QString targetFile = kernelPrefix.path() + "/" + QString::number(i) + '.' + kernelExtension;
      QFile::copy(kernelFile, targetFile);
      tempSpkKernels.append(targetFile);
    }

    // variables defined in TgoCassisModuleTests
    if (binaryCkKernels.size() == 0) {
      binaryCkKernels = generateBinaryKernels(tempCkKernels);
      binarySpkKernels = generateBinaryKernels(tempSpkKernels);

      binaryCkKernelsAsString = fileListToString(binaryCkKernels);
      binarySpkKernelsAsString = fileListToString(binarySpkKernels);
    }
  }


  void TgoCassisModuleKernels::TearDown() {
    binaryCkKernels = {};
    binarySpkKernels = {};
    binaryCkKernelsAsString = "";
    binarySpkKernelsAsString = "";
  }


  void HistoryBlob::SetUp() {
    TempTestingFiles::SetUp();

    std::istringstream hss(R"(
      Object = mroctx2isis
        IsisVersion       = "4.1.0  | 2020-07-01"
        ProgramVersion    = 2016-06-10
        ProgramPath       = /Users/acpaquette/repos/ISIS3/build/bin
        ExecutionDateTime = 2020-07-01T16:48:40
        HostName          = Unknown
        UserName          = acpaquette
        Description       = "Import an MRO CTX image as an Isis cube"

        Group = UserParameters
          FROM    = /Users/acpaquette/Desktop/J03_045994_1986_XN_18N282W.IMG
          TO      = /Users/acpaquette/Desktop/J03_045994_1986_XN_18N282W_isis.cub
          SUFFIX  = 18
          FILLGAP = true
        End_Group
      End_Object)");

    hss >> historyPvl;

    std::ostringstream ostr;
    ostr << historyPvl;
    std::string histStr = ostr.str();

    historyBlob = Blob("IsisCube", "History");
    historyBlob.setData(histStr.c_str(), histStr.size());
  }

  void RawPvlKeywords::SetUp() {
    keywordsToTry = {
      "KEYWORD",
      "KEYWORD X",
      "KEYWORD =",
      "KEYWORD = SOME_VAL",
      "KEYWORD = \"  val  \"",
      "KEYWORD = \" 'val' \"",
      "KEYWORD = (VAL",
      "KEYWORD = (VAL1,VAL2",
      "KEYWORD = (A B,C,D)",
      "KEYWORD = ((A B),(C),(D",
      "KEYWORD = (SOME_VAL)",
      "KEYWORD = (SOME_VAL) <a>",
      "KEYWORD=(SOME_VAL)<a>",
      "KEYWORD = (A, )",
      "KEYWORD = ()",
      "KEYWORD = (A,B)",
      "KEYWORD = {A, B}",
      "KEYWORD = (A,B) #comment this",
      "KEYWORD = ( A , B )",
      "KEYWORD\t=\t( A\t,\tB )",
      "KEYWORD = (A, B,C,D,E))",
      "KEYWORD = ((1, 2), {3,  4}, (5), 6)",
      "KEYWORD = { \"VAL1\" ,   \"VAL2\", \"VAL3\"}",
      "KEYWORD = { \"VAL1\" , \"VAL2\", \"VAL3\")",
      "KEYWORD = { \"VAL1\" ,",
      "KEYWORD = \"(A,B,\"",
      "KEYWORD = ',E)'",
      "KEYWORD = ((1,2))",
      "KEYWORD = (\"(f1+f2)\",\"/(f1-f2)\")",
      "KEYWORD = \"(F1+F2)/(F1-F2)\"",
      "KEYWORD = ( (1,2)  , (A,B) )",
      "KEYWORD = \"(f1 + min(f2,f3))\"",
      "KEYWORD = \"(min(f2,f3) + f1)\"",
      "KEYWORD = \"min(f2,f3) + f1\"",
      "KEYWORD = \"f1 + min(f2,f3)\"",
      "KEYWORD = (A <a>, B <b>, C, D <d>)",
      "KEYWORD = (A <a>, B <b>, C, D <d>) <e>",
      "KEYWORD = ',E) <unit>",
      "KEYWORD = ,E) <unit>",
      "#SOMECOMMENT\nKEYWORD = SOME_VAL",
      "#SOMECOMMENT1\n#SOMECOMMENT2\nKEYWORD = SOME_VAL",
      "//SOMECOMMENT1\n#SOMECOMMENT2\nKEYWORD = SOME_VAL",
      "/*SOMECOMMENT1*/\nKEYWORD = SOME_VAL",
      "KEYWORD = '/*\n*/'",
      "/* SOMECOMMENT1\n  SOMECOMMENT2\nSOMECOMMENT3 */\nKEYWORD = SOME_VAL",
      "/*C1\n\nA\n/*\nC3*/\nKEYWORD = SOME_VAL",
      "/*C1\n/**/\nKEYWORD = SOME_VAL",
      "/*C1\nA/**/\nKEYWORD = SOME_VAL",
      "/*           A            */\n/* B *//*C*/\nKEYWORD = SOME_VAL",
      "/*C1/**/\nKEYWORD = SOME_VAL",
      "/*C1   \n\nA\n\nC3*//*Neato*//*Man*/KEYWORD = (A,B,C) /*Right?\nYes!*/"
    };

    results = {
      PvlKeyword("KEYWORD"),                      // 0
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 1
      PvlKeyword("KEYWORD", "  val  "),           // 2
      PvlKeyword("KEYWORD", "  \'val\'  "),       // 3
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 4
      PvlKeyword("KEYWORD", "SOME_VAL", "a"),     // 5
      PvlKeyword("KEYWORD", "SOME_VAL", "a"),     // 6
      PvlKeyword("KEYWORD"),                      // 7
      PvlKeyword("KEYWORD"),                      // 8
      PvlKeyword("KEYWORD"),                      // 9
      PvlKeyword("KEYWORD"),                      // 10
      PvlKeyword("KEYWORD"),                      // 11
      PvlKeyword("KEYWORD"),                      // 12
      PvlKeyword("KEYWORD"),                      // 13
      PvlKeyword("KEYWORD"),                      // 14
      PvlKeyword("KEYWORD", "(A,B,"),             // 15
      PvlKeyword("KEYWORD", ",E)"),               // 16
      PvlKeyword("KEYWORD", "(1,2)"),             // 17
      PvlKeyword("KEYWORD"),                      // 18
      PvlKeyword("KEYWORD", "(F1+F2)/(F1-F2)"),   // 19
      PvlKeyword("KEYWORD"),                      // 20
      PvlKeyword("KEYWORD", "(f1 + min(f2,f3))"), // 21
      PvlKeyword("KEYWORD", "(min(f2,f3) + f1)"), // 22
      PvlKeyword("KEYWORD", "min(f2,f3) + f1"),   // 23
      PvlKeyword("KEYWORD", "f1 + min(f2,f3)"),   // 24
      PvlKeyword("KEYWORD"),                      // 25
      PvlKeyword("KEYWORD"),                      // 26
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 27
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 28
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 29
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 30
      PvlKeyword("KEYWORD", "/*\n*/"),            // 31
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 32
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 33
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 34
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 35
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 36
      PvlKeyword("KEYWORD", "SOME_VAL"),          // 37
      PvlKeyword("KEYWORD"),                      // 38
    };

    results[8].addValue("A");
    results[8].addValue("B");

    results[9].addValue("A");
    results[9].addValue("B");

    results[10].addValue("A");
    results[10].addValue("B");
    results[10].addComment("#comment this");

    results[11].addValue("A");
    results[11].addValue("B");

    results[12].addValue("A");
    results[12].addValue("B");

    results[13].addValue("(1, 2)");
    results[13].addValue("{3, 4}");
    results[13].addValue("(5)");
    results[13].addValue("6");

    results[14].addValue("VAL1");
    results[14].addValue("VAL2");
    results[14].addValue("VAL3");

    results[18].addValue("(f1+f2)");
    results[18].addValue("/(f1-f2)");

    results[20].addValue("(1,2)");
    results[20].addValue("(A,B)");

    results[25].addValue("A", "a");
    results[25].addValue("B", "b");
    results[25].addValue("C");
    results[25].addValue("D", "d");

    results[26].addValue("A", "a");
    results[26].addValue("B", "b");
    results[26].addValue("C", "e");
    results[26].addValue("D", "d");

    results[27].addComment("#SOMECOMMENT");

    results[28].addComment("#SOMECOMMENT1");
    results[28].addComment("#SOMECOMMENT2");

    results[29].addComment("//SOMECOMMENT1");
    results[29].addComment("#SOMECOMMENT2");

    results[30].addComment("/* SOMECOMMENT1 */");

    results[32].addComment("/* SOMECOMMENT1 */");
    results[32].addComment("/* SOMECOMMENT2 */");
    results[32].addComment("/* SOMECOMMENT3 */");

    results[33].addComment("/* C1 */");
    results[33].addComment("/* A  */");
    results[33].addComment("/*    */");
    results[33].addComment("/* C3 */");

    results[34].addComment("/* C1  */");
    results[34].addComment("/*     */");

    results[35].addComment("/* C1  */");
    results[35].addComment("/* A/* */");

    results[36].addComment("/*           A            */");
    results[36].addComment("/* B *//*C                */");

    results[37].addComment("/* C1/* */");

    results[38].addValue("A");
    results[38].addValue("B");
    results[38].addValue("C");
    results[38].addComment("/* C1    */");
    results[38].addComment("/* A     */");
    results[38].addComment("/* C3    */");
    results[38].addComment("/* Neato */");
    results[38].addComment("/* Man   */");
    results[38].addComment("/*Right? Yes!*/");

    valid = {
      true,
      false,
      false,
      true,
      true,
      true,
      false,
      false,
      false,
      false,
      true,
      true,
      true,
      false,
      true,
      true,
      true,
      true,
      true,
      true,
      false,
      true,
      true,
      false,
      false,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      false,
      false,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true};
  }

  void RawPvlKeywords::TearDown() {}
}
