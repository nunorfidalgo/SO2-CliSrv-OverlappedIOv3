#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>

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

DWORD WINAPI ThreadClienteReader(LPVOID lpvParam);

int DeveContinuar = 1;
int ReaderAlive = 0;

int _tmain(int argc, TCHAR *argv[]) {
	HANDLE	hPipe;
	BOOL	fSuccess = FALSE;
	DWORD	cbWritten, dwMode;
	LPTSTR	lpszPipename = TEXT("\\\\.\\pipe\\piexemplo");

	Msg		MsgToSend;
	HANDLE	hThread;
	DWORD	dwThreadId = 0;

	_setmode(_fileno(stdout), _O_WTEXT);

	_tprintf(TEXT("Escreve nome > "));
	readTChars(MsgToSend.quem, QUEMSZ);

	while (1) {
		hPipe = CreateFile(
			lpszPipename,
			GENERIC_READ | GENERIC_WRITE,
			0 | FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0 | FILE_FLAG_OVERLAPPED,
			NULL);

		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		if (GetLastError() != ERROR_PIPE_BUSY) {
			_tprintf(TEXT("\nCreate file deu erro e não foi BUSY. Erro=%d"), GetLastError());
			pressEnter();
			return -1;
		}

		if (!WaitNamedPipe(lpszPipename, 30000)) {
			_tprintf(TEXT("Esperei por uma instância durante 30 segundos. Desisto. Sair"));
			pressEnter();
			return -1;
		}
	}

	dwMode = PIPE_READMODE_MESSAGE;
	fSuccess = SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL);
	if (!fSuccess) {
		_tprintf(TEXT("SetNamedPipeHandleState falhou. Erro=%d"), GetLastError());
		pressEnter();
		return -1;
	}

	hThread = CreateThread(NULL, 0, ThreadClienteReader, (LPVOID)hPipe, 0, &dwThreadId);
	if (hThread = NULL) {
		_tprintf(TEXT("\nErro na criação da thread. Erro=%d"), GetLastError());
		return -1;
	}

	HANDLE WriteReady;
	OVERLAPPED OverlWr = { 0 };

	WriteReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (WriteReady == NULL) {
		_tprintf(TEXT("\nCliente: não foi possível criar o Evento. Mais vale parar já."));
		return 1;
	}

	_tprintf(TEXT("\nligação estabelecida. \"exit\" para sair"));
	while (1) {
		_tprintf(TEXT("\n%s > "), MsgToSend.quem);
		readTChars(MsgToSend.msg, MSGTXTSZ);
		if (_tcscmp(TEXT("exit"), MsgToSend.msg) == 0)
			break;

		_tprintf(TEXT("\nA enviar %d bytes: \"%s\""), Msg_Sz, MsgToSend.msg);
		ZeroMemory(&OverlWr, sizeof(OverlWr));
		ResetEvent(WriteReady);
		OverlWr.hEvent = WriteReady;

		fSuccess = WriteFile(hPipe, &MsgToSend, Msg_Sz, &cbWritten, &OverlWr);
		WaitForSingleObject(WriteReady, INFINITE);
		_tprintf(TEXT("\nWrite concluido"));

		GetOverlappedResult(hPipe, &OverlWr, &cbWritten, FALSE);
		if (cbWritten < Msg_Sz)
			_tprintf(TEXT("\nWrite TALVEZ falhou. Erro=%d"), GetLastError());

		_tprintf(TEXT("\nMensagem enviada"));
	}

	_tprintf(TEXT("\nEncerrar a thread ouvinte"));

	DeveContinuar = 0;

	if (ReaderAlive) {
		WaitForSingleObject(hThread, 3000);
		_tprintf(TEXT("\nThread reader encerread ou timeout"));
	}

	_tprintf(TEXT("\nCliente vai terminar ligação e sair"));

	CloseHandle(WriteReady);
	CloseHandle(hPipe);
	pressEnter();
	return 0;
}

DWORD WINAPI ThreadClienteReader(LPVOID lpvParam) {
	Msg			FromServer;

	DWORD		cbBytesRead = 0;
	BOOL		fSuccess = FALSE;
	HANDLE		hPipe = (HANDLE)lpvParam;

	HANDLE		ReadReady;
	OVERLAPPED	OverlRd = { 0 };

	if (hPipe == NULL) {
		_tprintf(TEXT("\nThread Reader - o handle recebido no param da thread é mulo\n"));
		return -1;
	}

	ReadReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ReadReady == NULL) {
		_tprintf(TEXT("\nCliente: não foi possível criar o Evento Read. Mais valo para já."));
		return 1;
	}

	ReaderAlive = 1;
	_tprintf(TEXT("Thread Reader - a receber mensagem\n"));

	while (DeveContinuar) {
		ZeroMemory(&OverlRd, sizeof(OverlRd));
		OverlRd.hEvent = ReadReady;
		ResetEvent(ReadReady);

		fSuccess = ReadFile(hPipe, &FromServer, Msg_Sz, &cbBytesRead, &OverlRd);
		WaitForSingleObject(ReadReady, INFINITE);
		_tprintf(TEXT("\nRead Concluido"));

		GetOverlappedResult(hPipe, &OverlRd, &cbBytesRead, FALSE);
		if (cbBytesRead < Msg_Sz)
			_tprintf(TEXT("\nReadFile falhou. Erro=%d"), GetLastError());

		_tprintf(TEXT("\nServidor disse: [%s]"), FromServer.msg);
	}

	ReaderAlive = 0;
	_tprintf(TEXT("Thread Reader a terminar. \n"));
	return 1;
}