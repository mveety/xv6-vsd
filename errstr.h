// how big is the biggest possible errstr?
#define ERRMAX 256

// easy macros
#define seterr(x) werrstr(x, sizeof(x))
#define fail(check, err, retv) if(check){werrstr(err, sizeof(err)); return retv;}

// list of standard error strings
// test errstrs
#define EROBPIKE "object oriented design is the roman numerals of computing"
#define EAIJU "no."
#define EMVEETY "what?"
#define EIDKMAN "general error"

// kernel/misc errors
#define EKBADSYSCALL "system call does not exist or is not implemented"
#define EKNOMEM "system is out of memory"
#define EKBADARG "unknown arguments given to system call"
#define EKDISABLED "system call is disabled"
#define EKKVMFAIL "setupkvm() failed"
#define EKBADUVMALLOC "allocuvm failed"
#define EKBADUVMLOAD "loaduvm failed"

// device errors
#define EDNOTEXIST "the requested device does not exist"
#define EDNOREAD "could not read from device"
#define EDNOWRITE "could not write to device"
#define EDFUCKYOU "fuck you."
#define EDSTARTFAIL "could not start driver"
#define EDBADCMD "bad driver command"
#define EDTOOLONG "nbytes+boff > BSIZE"

// security errors
#define ESNOPERMS "not allowed"
#define ESNOTOWN "not owner of the file"
#define ESCANTOPEN "not owner or allowed"
#define ESBADSYSCALL "not enough permissions to run syscall"
#define ESBADUSER "user does not exist"
#define ESBADPASS "password does not exist"
#define ESBADAUTH "syntax error in password"
#define ESNOFREEUSER "no free slots in users table"
#define ESUNAMETOOLONG "given username is too long"
#define ESUNAMETOOSHORT "given username is too short"
#define ESUNAMEGIRTH "given username is too girthy"
#define ESNOUSER "user does not exist"
#define ES2001 "I'm sorry Dave"

// general syscalls
#define ESCBADARGS "Missing or malformed syscall arguments"

// proc syscalls
#define EPFORKNP "no free processes"
#define EPCLONENP "no free processes"
#define EPFORKNS "could not copy parent state"
#define EPWAITNOCH "no children's deaths to mourn"
#define EPNOARGV "no known arguments"
#define EPCANTOPEN "cannot open file"
#define EPCANTFIND "cannot find file"
#define EPCANTREAD "short read"
#define EPEXECFAIL "exec failed"
#define EPTHREADEXEC "threads are unable to exec"
#define EPNOTHREADS "threading is disabled"
#define EP2MANYT "parent has maximum number of child threads"
#define EPBADMAGIC "not an executable"
#define EPBADHEADERREAD "bad header read"
#define EPFILETOOBIG "elf memsz < filesz"
#define EPTOOMANYARGS "argv is more than MAXARG"

// io syscalls
#define EILNDIR "cannot make links of directories"
#define EINOTOPEN "the file is not open"
#define EIEOF "end of file"
#define EIUNREAD "file is unreadable"
#define EIUNWRITE "file is unwriteable"
#define EISHORTWRITE "a short write to file occured"
#define EIEXIST "file exists"
#define EIFUNKNOWN "fd type is unknown"
#define EIDIFFDEV "can not link across devices"
#define EINOPARENT "target inode has no parent"
#define EIFSNOINODE "filesystem full: could not alloc inode"
#define EIFSFULL "filesystem full: bitmap full"
#define EIFSALLOC "cannot allocate file"
#define EINODOTS "unable to create links . and .."
#define EICANTOPEN "cannot open file"
#define EICANTFIND "cannot find file"
#define EINOCREATE "cannot create file"
#define EINOUNLINK "cannot unlink file"
#define EINOLINK "cannot link file"
#define EIDIRFULL "directory not empty"
#define EIBADTYPE "file is the wrong type"
#define EIBOFFSET "read offset is out of bounds"
