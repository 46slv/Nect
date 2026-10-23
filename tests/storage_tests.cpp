#include "storage.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using namespace nect;
using namespace nect::desktop;
namespace {
int checks=0;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
template<class F>void rejects(const char* code,F action) {
    try{action();}catch(const Error& error){check(error.code==code,("Expected "+std::string(code)+", got "+error.code).c_str());return;}
    throw std::runtime_error("Expected rejection: "+std::string(code));
}
QByteArray read(const QString& path) {
    QFile file(path);check(file.open(QIODevice::ReadOnly),"Read test file");return file.readAll();
}
void external_write(const QString& path,const QByteArray& bytes) {
    QFile file(path);check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Open external test writer");
    check(file.write(bytes)==bytes.size(),"Write external test bytes");file.close();
}
QByteArray native(int value) {
    auto document=empty_document("storage-doc","comp","art");document.compositions.front().name="Revision "+std::to_string(value);
    return QByteArray::fromStdString(encode(document));
}
QStringList owned(const QString& path) {
    const QRegularExpression pattern("^[0-9]{8}-[0-9]{9}-[0-9a-f]{64}\\.nect$");QStringList result;
    for(const auto& name:QDir(path+".backups").entryList(QDir::Files,QDir::Name))if(pattern.match(name).hasMatch())result<<name;
    return result;
}
#ifdef _WIN32
class DenyReplacement {
    HANDLE handle_=INVALID_HANDLE_VALUE;
public:
    explicit DenyReplacement(const QString& path) {
        handle_=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(handle_!=INVALID_HANDLE_VALUE,"Open Windows handle that denies replacement and writes");
    }
    ~DenyReplacement(){if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);}
};
#endif
void stamps_and_conflicts(const QString& directory) {
    const auto path=directory+"/native.nect";const auto first=native(1),second=native(2);
    const FileStamp absent;const auto initial=store_native(path,first,absent,true);
    check(initial.exists&&initial.sha256==QCryptographicHash::hash(first,QCryptographicHash::Sha256),"Save stamp hashes the actual byte payload with SHA256");
    check(native_path(directory+"/./native.nect")==native_path(path),"Path aliases resolve to the same canonical lock target");
    check(!QFileInfo::exists(path+".lock"),"Successful save releases its cooperative lock");
    const auto loaded=load_native(path);check(loaded.stamp==initial&&loaded.document.compositions.front().name=="Revision 1","Native load returns document and matching on-disk stamp");
    const auto spelling="\n  "+first+"\n";external_write(path,spelling);
    const auto spelled=load_native(path);
    check(spelled.document==loaded.document&&spelled.stamp.sha256==QCryptographicHash::hash(spelling,QCryptographicHash::Sha256)&&spelled.stamp!=initial,
        "Load hashes source bytes rather than re-encoding the decoded document");
    rejects("FILE_CHANGED",[&]{store_native(path,second,initial,true);});
    check(read(path)==spelling&&owned(path).isEmpty(),"External change rejects before backup or replacement");
    const auto changed=store_native(path,second,spelled.stamp,true);
    check(read(path)==second&&owned(path).size()==1,"Matching actual-byte expectation allows the next save");
    check(read(QDir(path+".backups").filePath(owned(path).front()))==spelling,"Backup keeps exact previous spelling, not normalized native JSON");
    const auto count=owned(path).size();check(store_native(path,second,changed,true)==changed&&owned(path).size()==count,"Saving identical bytes creates no duplicate backup");
    check(QFile::remove(path),"Delete native externally");
    rejects("FILE_CHANGED",[&]{store_native(path,first,changed,true);});
    check(!QFileInfo::exists(path),"External deletion does not silently recreate the current native");
    const auto recreated=store_native(path,first,std::nullopt,true);check(read(path)==first&&recreated==initial,"Explicit Save As without expectation permits recreation");
    rejects("FILE_CHANGED",[&]{store_native(path,second,FileStamp{},true);});
    QLockFile lock(native_path(path)+".lock");lock.setStaleLockTime(0);check(lock.tryLock(0),"Hold cooperative save lock");
    rejects("FILE_LOCKED",[&]{store_native(directory+"/./native.nect",second,recreated,true);});
    check(read(path)==first,"A cooperating lock prevents replacement through a path alias");lock.unlock();
    const auto before=read(path);rejects("IO_ERROR",[&]{store_native(directory+"/missing/native.nect",second,std::nullopt,true);});
    check(read(path)==before,"Invalid destination does not affect the prior document");
    rejects("OUTPUT_LIMIT",[&]{store_native(path,QByteArray(64*1024*1024+1,' '),recreated,true);});
    check(read(path)==before,"Oversized write rejects before changing disk or history");
    const auto huge=directory+"/huge.nect";external_write(huge,QByteArray(64*1024*1024+1,' '));
    rejects("INPUT_LIMIT",[&]{load_native(huge);});
    const auto boundary=directory+"/boundary.nect";auto exact=first;exact.append(QByteArray(64*1024*1024-exact.size(),' '));
    const auto exact_stamp=store_native(boundary,exact,FileStamp{},false);
    check(load_native(boundary).stamp==exact_stamp&&read(boundary).size()==64*1024*1024,"Exactly 64 MiB remains within the native storage limit");
}
void retention_and_restore(const QString& directory) {
    const auto path=directory+"/retained.nect";auto current=store_native(path,native(0),FileStamp{},true);
    check(QDir().mkpath(path+".backups"),"Create backup fixture directory");
    const auto legacy=path+".backups/20260920-120000000-legacy-uuid.nect";external_write(legacy,native(-1));
    const auto unrelated=path+".backups/user-copy.nect";external_write(unrelated,native(-2));
    for(int i=1;i<=14;++i)current=store_native(path,native(i),current,true);
    auto generations=owned(path);check(generations.size()==10,"Retention keeps ten owned backup generations after verified replacement");
    check(read(legacy)==native(-1)&&read(unrelated)==native(-2),"Retention neither counts nor prunes legacy and unrecognized backup files");
    check(read(QDir(path+".backups").filePath(generations.front()))==native(4)&&read(QDir(path+".backups").filePath(generations.back()))==native(13),
        "Chronological backup naming retains the most recent previous values even during rapid writes");
    const auto restore_path=QDir(path+".backups").filePath(generations.front());const auto restored=load_native(restore_path);
    check(restored.document.compositions.front().name=="Revision 4","A backup opens as the actual earlier authored document");
    const auto restore_bytes=read(restore_path);current=store_native(path,restore_bytes,current,true);
    check(load_native(path).document==restored.document&&read(path)==restore_bytes&&owned(path).size()==10,"Restoring backup bytes is a real verified save that also preserves the replaced version");
    const auto recovery=directory+"/recovery.nect";auto recovery_stamp=store_native(recovery,native(1),FileStamp{},false);
    store_native(recovery,native(2),recovery_stamp,false);check(!QFileInfo::exists(recovery+".backups"),"Recovery caller can replace without producing native backup generations");
}
void failures(const QString& directory) {
    const auto path=directory+"/failure.nect";auto current=store_native(path,native(0),FileStamp{},true);
    const auto backup_directory=path+".backups";external_write(backup_directory,"Directory blocked by file");
    rejects("IO_ERROR",[&]{store_native(path,native(1),current,true);});
    check(read(path)==native(0),"Backup creation failure leaves the existing native intact");
    check(QFile::remove(backup_directory),"Remove owned backup-directory blocker");
#ifdef _WIN32
    {
        DenyReplacement locked(path);
        rejects("IO_ERROR",[&]{store_native(path,native(1),current,true);});
        rejects("IO_ERROR",[&]{store_native(path,native(2),current,true);});
        check(read(path)==native(0)&&owned(path).size()==1,"Denied Windows replacement keeps native intact and failed retries deduplicate the exact prior backup");
    }
    current=store_native(path,native(1),current,true);check(read(path)==native(1)&&owned(path).size()==1,"Retry after releasing the Windows handle saves successfully without duplicate generations");
    for(int i=2;i<=10;++i)current=store_native(path,native(i),current,true);
    const auto oldest=QDir(path+".backups").filePath(owned(path).front());
    {
        DenyReplacement backup_locked(oldest);
        current=store_native(path,native(11),current,true);
        check(read(path)==native(11)&&load_native(path).stamp==current&&owned(path).size()==11&&QFileInfo::exists(oldest),
            "A locked old generation leaves excess backup retention while the verified native save still succeeds");
    }
    current=store_native(path,native(12),current,true);check(owned(path).size()==10,"A later successful save resumes best-effort retention after the old generation unlocks");
#else
    check(QDir().mkpath(directory+"/directory-target.nect"),"Create unwritable regular-file destination fixture");
    rejects("IO_ERROR",[&]{store_native(directory+"/directory-target.nect",native(1),std::nullopt,false);});
    check(read(path)==native(0),"Invalid regular-file destination preserves the stable source");
#endif
}
int interrupted_child(const QString& path) {
    QSaveFile file(native_path(path));file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||file.write("partial replacement")!=19||!file.flush())return 3;
    std::cout<<"STAGED\n"<<std::flush;
    return QCoreApplication::exec(); // Parent kills before commit; no production failure hook.
}
void interruption(const QString& directory) {
    const auto path=directory+"/interrupted.nect";const auto initial=native(42);const auto current=store_native(path,initial,FileStamp{},true);
    QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--interrupt-stage",path});
    check(child.waitForStarted(5000),"Start isolated QSaveFile interruption helper");
    if(!child.waitForReadyRead(5000)){child.kill();child.waitForFinished(5000);throw std::runtime_error("Staging helper did not become ready");}
    const auto ready=child.readAllStandardOutput();
    if(!ready.contains("STAGED")){child.kill();child.waitForFinished(5000);throw std::runtime_error("Staging helper did not report staged bytes");}
    child.kill();check(child.waitForFinished(5000),"Terminate staged helper before atomic commit");
    check(read(path)==initial&&load_native(path).stamp==current,"Process interruption after QSaveFile staging preserves the previous native bytes");
    store_native(path,native(43),current,true);check(load_native(path).document.compositions.front().name=="Revision 43","Normal save remains usable after an interrupted staging process");
    std::cout<<"Interruption boundary: test-only helper uses QSaveFile with direct fallback disabled; killed after staged flush, before commit. Power-loss durability is not measured.\n";
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().size()==3&&app.arguments()[1]=="--interrupt-stage")return interrupted_child(app.arguments()[2]);
    try {
        QTemporaryDir temporary;check(temporary.isValid(),"Create owned storage test directory");
        stamps_and_conflicts(temporary.path());retention_and_restore(temporary.path());failures(temporary.path());interruption(temporary.path());
        std::cout<<"PASS "<<checks<<" native storage checks\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
