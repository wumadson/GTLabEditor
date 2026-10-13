#include "autoSyncCoordinator.h"
#include "patchTransferCodec.h"
#include <QtTest>
#include <QFile>

class CoordinatorTests : public QObject {
    Q_OBJECT
    QString wire;
    AutoSyncCoordinator::State clean;
private slots:
    void initTestCase() {
        QFile f(QString(TEST_SOURCE_DIR)+"/gt-10 temp patch_trimmed.syx"); QVERIFY(f.open(QIODevice::ReadOnly));
        wire=QString::fromLatin1(f.read(1784).toHex().toUpper());
        // The fixture is User/file memory; use the validated Temporary Buffer address.
        QByteArray bytes=QByteArray::fromHex(wire.toLatin1());
        int cursor=0;
        while(cursor<bytes.size()) {
            int end=bytes.indexOf(char(0xF7),cursor); QVERIFY(end>cursor);
            bytes[cursor+7]=char(0x60);bytes[cursor+8]=0;
            int sum=0;for(int i=cursor+7;i<end-1;++i)sum+=uchar(bytes[i]);
            bytes[end-1]=char((128-(sum%128))%128);cursor=end+1;
        }
        wire=QString::fromLatin1(bytes.toHex().toUpper());
        const auto d=PatchTransferCodec::decodePatchReply(wire); QVERIFY(d.valid);
        clean.source=d.logicalBlocks00To0C;
        clean.connected=clean.ready=clean.sourceValid=clean.baselineValid=true;
        clean.exclusiveAvailable=clean.transportValid=true;
    }
    void admission_data() {
        QTest::addColumn<QString>("gate");QTest::addColumn<QString>("expected");
        QTest::newRow("clean-unknown-identity")<<"clean"<<"";
        QTest::newRow("dirty")<<"dirty"<<"DIRTY";
        QTest::newRow("manual-read-write-system-chain-backup-quick")<<"busy"<<"BUSY";
        QTest::newRow("exclusive")<<"exclusive"<<"BUSY";
        QTest::newRow("no-baseline")<<"baseline"<<"BASELINE";
        QTest::newRow("no-source")<<"source"<<"SOURCE";
        QTest::newRow("not-ready")<<"ready"<<"NOT_READY";
        QTest::newRow("invalid-transport")<<"transport"<<"TRANSPORT";
    }
    void admission() {
        QFETCH(QString,gate);QFETCH(QString,expected);auto s=clean;
        if(gate=="dirty")s.dirty=true;if(gate=="busy")s.operationsBusy=true;
        if(gate=="exclusive")s.exclusiveAvailable=false;if(gate=="baseline")s.baselineValid=false;
        if(gate=="source")s.sourceValid=false;if(gate=="ready")s.ready=false;
        if(gate=="transport")s.transportValid=false;
        int requests=0; AutoSyncCoordinator c([&]{return s;},[&](quint64){++requests;});
        QCOMPARE(c.requestCandidateRead("test"),expected.isEmpty());
        QCOMPARE(requests,expected.isEmpty()?1:0);
        if(!expected.isEmpty())QCOMPARE(c.lastResult().reason,expected);
    }
    void singleFlightAndStale() {
        int requests=0;quint64 token=0;
        AutoSyncCoordinator c([&]{return clean;},[&](quint64 t){++requests;token=t;});
        QVERIFY(c.requestCandidateRead("test"));QVERIFY(!c.requestCandidateRead("overlap"));
        QCOMPARE(c.lastResult().reason,QString("SINGLE_FLIGHT")); QCOMPARE(requests,1);
        c.completed(token+1);c.reply(token+1,wire);c.finished(token+1);QVERIFY(c.isBusy());
        c.completed(token);c.reply(token,wire);c.finished(token);
        QCOMPARE(c.lastResult().status,AutoSyncCoordinator::Unchanged);QVERIFY(!c.isBusy());
        const auto old=token;QVERIFY(c.requestCandidateRead("second"));
        c.completed(old);c.reply(old,wire);c.finished(old);QVERIFY(c.isBusy());
        c.completed(token);c.reply(token,wire);c.finished(token);
        QCOMPARE(c.lastResult().status,AutoSyncCoordinator::Unchanged);
    }
    void transportGates() {
        QVERIFY(AutoSyncCoordinator::transportReady(true,true,true,false,false));
        QVERIFY(!AutoSyncCoordinator::transportReady(true,true,true,true,true));
        QVERIFY(!AutoSyncCoordinator::transportReady(true,false,true,false,true));
        QVERIFY(!AutoSyncCoordinator::transportReady(true,true,false,false,true));
        QVERIFY(AutoSyncCoordinator::transportReady(false,false,false,true,true));
        QVERIFY(!AutoSyncCoordinator::transportReady(false,true,true,false,false));
    }
    void changed() {
        auto s=clean;const auto key=s.source.firstKey();
        s.source[key].replace(0,2,s.source[key].startsWith("00")?"01":"00");
        AutoSyncCoordinator c([&]{return s;},[](quint64){});
        QVERIFY(c.requestCandidateRead("test"));c.completed(1);c.reply(1,wire);c.finished(1);
        QCOMPARE(c.lastResult().status,AutoSyncCoordinator::Changed);
        QCOMPARE(c.lastResult().candidate.size(),13);QCOMPARE(c.lastResult().differingBytes,1);
        QCOMPARE(c.lastResult().differingBlocks,QStringList{key});
    }
    void invalidation_data() {
        QTest::addColumn<QString>("change");
        for(const char *c:{"session","identity","edit","event","duplicate","order","invalid"})QTest::newRow(c)<<QString(c);
    }
    void invalidation() {
        QFETCH(QString,change);auto s=clean;
        AutoSyncCoordinator c([&]{return s;},[](quint64){});QVERIFY(c.requestCandidateRead("test"));
        if(change=="order")c.reply(1,wire);
        c.completed(1);if(change=="duplicate")c.completed(1);
        c.reply(1,change=="invalid"?QString("bad"):wire);
        if(change=="session")++s.session;if(change=="identity")++s.patch;
        if(change=="edit")s.dirty=true;if(change=="event")c.invalidate("LOCAL_EDIT_THEN_REVERT");
        c.finished(1);QCOMPARE(c.lastResult().status,AutoSyncCoordinator::Error);
        QVERIFY(c.lastResult().candidate.isEmpty());
    }
};
QTEST_GUILESS_MAIN(CoordinatorTests)
#include "autoSyncCoordinatorTests.moc"
