#include <ptlib.h>
#include <ptlib/pprocess.h>
#include "wapa_http_server.h"
using namespace std;

	
class WAPAHTTPDService : public PProcess
{
    PCLASSINFO(WAPAHTTPDService, PProcess);
private:
#ifdef PTRACING
	PTextFile *m_logfile;
	PString	m_logfilename;
	void ReopenLogFile();
#endif
	const PString GetArgumentsParseString() const;
	bool InitLogging(const PArgList &args);
public:
	WAPAHTTPDService();

    void Main();
};

PCREATE_PROCESS(WAPAHTTPDService);

WAPAHTTPDService::WAPAHTTPDService()
    : PProcess("WAPAHTTPDService", "main", 1, 0, ReleaseCode, 0),m_logfile(NULL)
{
}

#ifdef PTRACING
void WAPAHTTPDService::ReopenLogFile()
{
	if (!m_logfilename) {
		if(m_logfile)
		{
			PTRACE(1, "Trace logging stopped.");
		}
		PTrace::SetStream(&cerr); // redirect to cerr
		m_logfile = new PTextFile(m_logfilename, PFile::WriteOnly,PFile::Create);
		m_logfile->SetPosition(0,PFile::End);
		if (!m_logfile->IsOpen()) {
			cout << "Warning: could not open trace output file \""
				<< m_logfilename << '"' << endl;
			delete m_logfile;
			m_logfile = 0;
			return;
		}
		PTrace::SetStream(m_logfile); // redirect to logfile
	}
	else
		PTrace::SetStream(&cerr); // redirect to cerr
	PTRACE(1, "Trace logging started.");
}
#endif

bool WAPAHTTPDService::InitLogging(const PArgList &args)
{
#ifdef PTRACING
	PTrace::SetOptions(PTrace::DateAndTime | PTrace::Thread | PTrace::RotateDaily | PTrace::AppendToFile);
	if (args.HasOption('o')) {
		m_logfilename = args.GetOptionString('o')+".log";;
	}
	PTrace::SetLevel(args.GetOptionCount('t'));
#endif
	return TRUE;
}

const PString WAPAHTTPDService::GetArgumentsParseString() const
{
	return PString
		(
		 "c-config:"
#ifdef PTRACING
		 "t-trace."
		 "o-output:"
#endif
		 );
}

PSyncPoint exitSignal;

#if defined(__WINDOWS)
BOOL WINAPI WinCtrlHandlerProc(DWORD dwCtrlType)
{
	PTRACE(1, "Shutdown Event "<<dwCtrlType);
	switch(dwCtrlType)
	{
		case CTRL_C_EVENT: 
		case CTRL_CLOSE_EVENT: 
		case CTRL_BREAK_EVENT: 
			{
				exitSignal.Signal();
				return TRUE;
			}
	}
	return FALSE;
}
#elif defined(__LINUX)
void termination_handler (int signum)
{
	PTRACE(1, "Shutdown Event "<<signum);
	switch(signum)
	{
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
			{
				exitSignal.Signal();
				break;
			}
	}
}
#endif

void WAPAHTTPDService::Main()
{
	PString config_file_name = "wapa_httpd.cfg";
	PTrace::SetLevel(0);
	PArgList & args = GetArguments();
	args.Parse(GetArgumentsParseString());
	InitLogging(args);

	ReopenLogFile();

	PTRACE(0,GetName()<<" "<<GetVersion(true));

	if(args.HasOption('c'))
		config_file_name = args.GetOptionString('c');

	WAPAHTTPServer *http_server = new WAPAHTTPServer(config_file_name);

	PTRACE(1,"WAPA HTTP Server on port "<<http_server->GetParams().port);
	if(http_server->Init()==WR_OK)
	{
		PTRACE(1,"WAPA HTTP Server initialized");

	#if defined(__WINDOWS)
		SetConsoleCtrlHandler(WinCtrlHandlerProc, TRUE);
	#elif defined(__LINUX)
		signal (SIGINT, termination_handler);
		signal (SIGHUP, termination_handler);
		signal (SIGTERM, termination_handler);
	#endif
		while (!exitSignal.Wait(1000))
		{
			http_server->Tick();
		}
		http_server->Deinit();
	}
	delete http_server;
#ifdef PTRACING
	PTrace::SetLevel(0);
	PTrace::SetStream(&cerr); // redirect to cerr
#endif
}

