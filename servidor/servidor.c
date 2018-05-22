#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

#include <io.h>
#include <fcntl.h>

#define BUFSIZE 2048

#define QUEMSZ 60
#define MSGTXTSZ 60

typedef struct {
	TCHAR quem[QUEMSZ];
	TCHAR msg[MSGTXTSZ];
} Msg;

#define Msg_Sz sizeof(Msg)

void readTChars(TCHAR *p, int maxchars) {
	int len;
	_fgetts(p, maxchars, stdin);
	len = _tcslen(p);
	if (p[len - 1] == TEXT('\n'))
		p[len - 1] = TEXT('\0');
}

void pressEnter() {
	TCHAR somekeys[25];
	_tprintf(TEXT("\nPress enter > "));
	readTChars(somekeys, 25);
}

DWORD WINAPI InstanceThread(LPVOID lpvParam);

#define MAXCLIENTES 10
HANDLE clientes[MAXCLIENTES];

void iniciaClientes() {
	int i;
	for (i = 0; i < MAXCLIENTES; i++)
		clientes[i] = NULL;
}

void adicionaCliente(HANDLE cli) {
	int i;
	for (i = 0; i < MAXCLIENTES; i++) {
		if (clientes[i] == NULL)
			clientes[i] = cli;
		return;
	}
}

void removeCliente(HANDLE cli) {
	int i;
	for (i = 0; i < MAXCLIENTES; i++) {
		if (clientes[i] == cli) {
			clientes[i] = NULL;
			return;
		}
	}
}

HANDLE WriteReady;

int writeClienteASINC(HANDLE hPipe, Msg msg) {
	DWORD cbWritten = 0;
	BOOL fSuccess = FALSE;
	OVERLAPPED OverlWr = { 0 };

	ZeroMemory(&OverlWr, sizeof(OverlWr));
	ResetEvent(WriteReady);
	OverlWr.hEvent = WriteReady;

	fSuccess = WriteFile(hPipe, &msg, Msg_Sz, &cbWritten, &OverlWr);
	WaitForSingleObject(WriteReady, INFINITE);
	GetOverlappedResult(hPipe, &OverlWr, &cbWritten, FALSE);
	if (cbWritten < Msg_Sz)
		_tprintf(TEXT("\nWriteFile não escreveu toda a informação. Erro=%d"), GetLastError());
	return 1;
}

int broadcastClientes(Msg msg) {
	int i, numwrites = 0;
	for (i = 0; i < MAXCLIENTES; i++)
		if (clientes[i] != 0)
			numwrites += writeClienteASINC(clientes[i], msg);
	return numwrites;
}

int _tmain(VOID) {
	BOOL fConnected = FALSE;
	DWORD dwThreadId = 0;
	HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\piexemplo");

	_setmode(_fileno(stdout), _O_WTEXT);

	WriteReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (WriteReady == NULL) {
		_tprintf(TEXT("\nServidor: não foi possível criar o evento Write. Mais vale parar já."));
		return 1;
	}

	iniciaClientes();

	while (1) {
		hPipe = CreateNamedPipe(
			lpszPipename,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			BUFSIZE,
			BUFSIZE,
			5000,
			NULL);

		if (hPipe == INVALID_HANDLE_VALUE) {
			_tprintf(TEXT("\nCreateNamedPipe falhou ,erro=%d"), GetLastError());
			return -1;
		}

		_tprintf(TEXT("\nServidor: a aguardar que um cliente se ligue..."));
		fConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (fConnected) {
			hThread = CreateThread(NULL, 0, InstanceThread, (LPVOID)hPipe, 0, &dwThreadId);
			if (hThread == NULL) {
				_tprintf(TEXT("\nErro na criação da thread. Erro=%d"), GetLastError());
				return -1;
			}
			else
				CloseHandle(hThread);
		}
		else
			CloseHandle(hPipe);
	}
	return 0;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam) {
	Msg Pedido, Resposta;
	DWORD cbBytesRead = 0, cbReplyBytes = 0;
	int numresp = 0;
	BOOL fSuccess = FALSE;
	HANDLE hPipe = (HANDLE)lpvParam;

	HANDLE ReadReady;
	OVERLAPPED OverlRd = { 0 };

	_tcscpy(Resposta.quem, TEXT("SRV"));

	if (hPipe == NULL) {
		_tprintf(TEXT("\nErro - o handle enviado no param da thread é nulo"));
		return -1;
	}

	ReadReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ReadReady == NULL) {
		_tprintf(TEXT("\nServidor: não foi possível criar o evento Read. Mais vale para já."));
		return -1;
	}

	adicionaCliente(hPipe);

	while (1) {
		ZeroMemory(&OverlRd, sizeof(OverlRd));
		ResetEvent(ReadReady);
		OverlRd.hEvent = ReadReady;

		fSuccess = ReadFile(hPipe, &Pedido, Msg_Sz, &cbBytesRead, &OverlRd);
		WaitForSingleObject(ReadReady, INFINITE);
		GetOverlappedResult(hPipe, &OverlRd, &cbBytesRead, FALSE);
		if (cbBytesRead < Msg_Sz)
			_tprintf(TEXT("\nReadFIle não leu os dados todos. Erro=%d"), GetLastError());

		_tprintf(TEXT("\nServidor: Recebi (?) de: [%s] msg: [%s]"), Pedido.quem, Pedido.msg);
		_tcscpy(Resposta.msg, Pedido.quem);
		_tcscat(Resposta.msg, TEXT(": "));
		_tcscat(Resposta.msg, Pedido.msg);
		numresp = broadcastClientes(Resposta);
		_tprintf(TEXT("\nServidor: %d respostas enviadas"), numresp);
	}
	removeCliente(hPipe);

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);
	_tprintf(TEXT("\nThread dedicada Cliente a terminar"));
	return 1;
}