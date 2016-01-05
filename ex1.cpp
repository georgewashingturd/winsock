#define _WIN32_WINNT  0x501

#include <windows.h>
#include <iostream>
#include <string>

#include <direct.h>
#include <stdio.h>
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <time.h>

#include "resource.h"

#define GetCurrentDir _getcwd
#define FailWithAction(cond, action, label) if (cond) {action; goto label;}
#define UpdateStatus(...) do{ sprintf(msg, __VA_ARGS__); UpdateStatusPanel(msg);}while(0)

#define DEFAULT_BUFLEN (512*16)
#define DEFAULT_PORT    "27015"
#define BUFSIZE         1024

using namespace std;


char ** fileList = NULL;
int numFiles;

char destIP[BUFSIZE];
char portNo[BUFSIZE];

DWORD serverDescriptor;
DWORD clientDescriptor;

HWND dlgHandle;
char * statusBuffer;

char msg[BUFSIZE];
char errMsg[BUFSIZE];
    
/*
string  GetFileName( const string & prompt ) { 
    const int BUFSIZE = 1024;
    char buffer[BUFSIZE] = {0};
    OPENFILENAME ofns = {0};
    ofns.lStructSize = sizeof( ofns );
    ofns.lpstrFile = buffer;
    ofns.nMaxFile = BUFSIZE;
    ofns.lpstrTitle = prompt.c_str();
    GetOpenFileName( & ofns );
    return buffer;
}
*/

char * PrintLastError(DWORD dw)
{ 
    // Retrieve the system error message for the last-error code

    char lpMsgBuf[BUFSIZE*16];

    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        BUFSIZE*16, 
        NULL );

    sprintf(errMsg, "Error %u:\r\n%s", (unsigned int)dw, (char *)lpMsgBuf); 

    return errMsg;
}


void UpdateStatusPanel(const char * message)
{
    int ptr = 0;
    if (statusBuffer == NULL)
        return;
    
    if (strlen(statusBuffer) + strlen(statusBuffer) > ((BUFSIZE * 1024 * 4) - 1))
        ptr = 0;
    else
        ptr = strlen(statusBuffer);
    
    sprintf(&statusBuffer[ptr], "%s", message);
    SetWindowText(GetDlgItem( dlgHandle, IDC_STATUS ), statusBuffer);
    PostMessage(GetDlgItem( dlgHandle, IDC_STATUS ),EM_LINESCROLL,0,(LPARAM)100000);
}

int getLocalAdaptersInfo(char * buffer)
{

    // Declare and initialize variables
    //
    // It is possible for an adapter to have multiple
    // IPv4 addresses, gateways, and secondary WINS servers assigned to the adapter
    //
    // Note that this sample code only prints out the
    // first entry for the IP address/mask, and gateway, and
    // the primary and secondary WINS server for each adapter

    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;
    UINT i;
    PIP_ADDR_STRING ipList;

    buffer[0] = '\0';

    ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);

    pAdapterInfo = (IP_ADAPTER_INFO *) malloc(sizeof (IP_ADAPTER_INFO));

    if (pAdapterInfo == NULL)
    {
        UpdateStatus("Error allocating memory needed to call GetAdaptersinfo()\r\n");
        return 1;
    }
    else
        UpdateStatus("Memory allocation for GetAdaptersinfo() call is OK!\r\n");

    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
    {
        UpdateStatus("Not enough buffer! Re-allocating...\r\n");
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen);
        if (pAdapterInfo == NULL)
        {
            UpdateStatus("Error allocating memory needed to call GetAdaptersinfo()\r\n");
            return 1;
        }
        else
            UpdateStatus("Memory allocation for GetAdaptersinfo() 2nd call is OK!\r\n");
    }
    else
        UpdateStatus("Buffer for GetAdaptersInfo() is OK!\r\n");

 

    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
    {
        pAdapter = pAdapterInfo;
        while (pAdapter)
        {            
            ipList = &(pAdapter->IpAddressList);
            while (ipList != NULL)
            {
                if (strcmp(ipList->IpAddress.String, "0.0.0.0") != 0)
                {
                    sprintf(&buffer[strlen(buffer)], "%s\r\n", pAdapter->Description);
#if 0
                    for (i = 0; i < pAdapter->AddressLength; i++)
                    {
                        if (i == (pAdapter->AddressLength - 1))
                            sprintf(&buffer[strlen(buffer)], "%.2X\r\n", (int) pAdapter->Address[i]);
                        else
                            sprintf(&buffer[strlen(buffer)], "%.2X-", (int) pAdapter->Address[i]);
                    }                    
#endif
                    sprintf(&buffer[strlen(buffer)], "%s\r\n", ipList->IpAddress.String);
                }
                                    
                ipList = ipList->Next;
            }
            
            pAdapter = pAdapter->Next;
        }
    }
    else
    {
        UpdateStatus("GetAdaptersInfo failed with error: %d\r\n", dwRetVal);
    }

    if (pAdapterInfo)
        free(pAdapterInfo);

    return 0;
}


char ** GetFileName( OPENFILENAME * ofns, char * buffer, int * numFiles, const string & prompt )
{
    *numFiles = 0;
    int i, j;
    char * ptr;

    if (ofns == NULL)
        return NULL;

    memset(ofns, 0, sizeof(OPENFILENAME));

    ofns->lStructSize = sizeof( OPENFILENAME );
    ofns->lpstrFile = buffer;
    ofns->nMaxFile = BUFSIZE;
    ofns->lpstrTitle = prompt.c_str();
    ofns->Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    ofns->hwndOwner = dlgHandle; // to make it modal

    GetOpenFileName( ofns );

    ptr = ofns->lpstrFile;
    ptr[ofns->nFileOffset-1] = 0;
    //printf("Directory: %s\n", ptr);
    ptr += ofns->nFileOffset;
    while (*ptr)
    {
        (*numFiles)++;
        //printf("File: %s\n", ptr);
        ptr += (strlen(ptr)+1);
    }

    
    fileList = (char **)malloc(2 * (*numFiles) * sizeof(char *));

    if (fileList == NULL)
        return NULL;

    ptr = ofns->lpstrFile;
    ptr[ofns->nFileOffset-1] = 0;
    ptr += ofns->nFileOffset;

    for(i = 0; i < (*numFiles)*2; i += 2)
    {
        fileList[i] = (char *)malloc(BUFSIZE * sizeof(char));
        fileList[i+1] = (char *)malloc(BUFSIZE * sizeof(char));

        // if it fails, free all previous pointers
        if (fileList[i] == NULL || fileList[i+1] == NULL)
        {
            for (j = i+1; j >= 0; j--)
            {
                free(fileList[j]);
            }
            
            free(fileList);
            return NULL;
        }
        sprintf(fileList[i], "%s", ptr);
        sprintf(fileList[i+1], "%s\\%s", ofns->lpstrFile, ptr);
        ptr += (strlen(ptr)+1);
    }

    return fileList;
}

// Thread worker for server
static DWORD WINAPI StartServer(void* serverArg)
{
    int iResult;
	DWORD ret = 1;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    char recvbuf[DEFAULT_BUFLEN*2];
    int recvbuflen = DEFAULT_BUFLEN;
    
    char folderName[128];
    
    // Winsock should have been initialized

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    UpdateStatus("Starting Server ...\r\n");

    // Resolve the server address and port
	FailWithAction( (iResult = getaddrinfo(NULL, portNo, &hints, &result)) != 0, UpdateStatus("getaddrinfo failed with error: %d\n", iResult), EXIT)

    // Create a SOCKET for connecting to server
    FailWithAction( (ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == INVALID_SOCKET, UpdateStatus("socket failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT)

    // Setup the TCP listening socket
    FailWithAction( bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR,  UpdateStatus("bind failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )
    freeaddrinfo(result);
    // Don't forget to do this as this might cause a crash if we free result twice
    result = NULL;

    FailWithAction( listen(ListenSocket, SOMAXCONN), UpdateStatus("listen failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )

    // Accept a client socket
    FailWithAction( (ClientSocket = accept(ListenSocket, NULL, NULL)) == INVALID_SOCKET, UpdateStatus("accept failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )

    // No longer need server socket
    closesocket(ListenSocket);

    // Receive until the peer shuts down the connection
    FILE * fo;
    int accumSizeRecv;
    do
    {        
        FailWithAction( (iResult = recv(ClientSocket, recvbuf, recvbuflen, 0)) < 0, UpdateStatus("recv failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )
        if (iResult > 0) 
        {
            // 0xBEEF0000 is indicator that the following data is the folder name
            if (((unsigned int *) recvbuf)[0] == 0xBEEF0000)
            {
                time_t     now = time(0);
                struct tm  tstruct;
                tstruct = *localtime(&now);

                strftime(folderName, sizeof(folderName), "%Y-%m-%d.%X", &tstruct);

                for (int i = 0; i < strlen(folderName); i++)
                {
                    if (folderName[i] == ':')
                        folderName[i] = '.';
                }
                UpdateStatus("Creating folder: %s\r\n", folderName);
                
                CreateDirectory(folderName, NULL);
            }
            // 0xBEEF0001 is indicator that the following data is the file name
            else if (((unsigned int *) recvbuf)[0] == 0xBEEF0001)
            {
                char fullPath[BUFSIZE];
                
                sprintf(fullPath, "%s\\%s", folderName, &recvbuf[4]);
                
                UpdateStatus("Receiving file name: %s\r\n", &recvbuf[4]);
                
                FailWithAction( (fo = fopen(fullPath, "wb")) == NULL, UpdateStatus("Error creating file\r\n"), EXIT )
                accumSizeRecv = 0;
            }
            // 0xBEEF0002 is indicator of end of file
            else if (((unsigned int *) recvbuf)[0] == 0xBEEF0002)
            {
                fclose(fo);
                UpdateStatus("recv complete: %d bytes\r\n", accumSizeRecv);
            }
            else if (((unsigned int *) recvbuf)[0] == 0xDEADBEEF && ((unsigned int *) recvbuf)[1] == 0xBEEFFACE)
            {
                fclose(fo);
                UpdateStatus("Transaction complete, closing connection ...\r\n", accumSizeRecv);
                
                //return 1;
                break;
            }
            else
            {
                accumSizeRecv += iResult;
                //printf("Bytes received: %d\n", accumSizeRecv);

                int writeLen;
                FailWithAction( (writeLen = fwrite(recvbuf, 1, iResult, fo)) != iResult, UpdateStatus("Write file failed"), EXIT )
            }
            
            // send valid response after every receive
            ((unsigned int *)recvbuf)[0] = 0xBEEFFACE;
            FailWithAction( send( ClientSocket, recvbuf, sizeof(unsigned int), 0 ) == SOCKET_ERROR, UpdateStatus("send response failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )
        }
        else if (iResult == 0)
            UpdateStatus("Connection closing...\n");
    } while (iResult > 0);

    // shutdown the connection since we're done
    FailWithAction( shutdown(ClientSocket, SD_SEND) == SOCKET_ERROR, UpdateStatus("shutdown failed with error!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )

	ret = 0;
	
// cleanup
EXIT:
	if (result != NULL)
		freeaddrinfo(result);

	closesocket(ListenSocket);
	closesocket(ClientSocket);

    EnableWindow(GetDlgItem( dlgHandle, IDC_START ), true);
    
    return ret;
}

bool IsServerResponseValid(SOCKET * ConnectSocket, char recvbuf[])
{
    int iResult;
    
    ((unsigned int *)recvbuf)[0] = 0x5A5A5A5A;
    iResult = recv(*ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
    if (((unsigned int *)recvbuf)[0] != 0xBEEFFACE)
    {
        UpdateStatus("Server response error!\r\n%s\r\n", PrintLastError(WSAGetLastError()));
        closesocket(*ConnectSocket);
        return false;
    }
    return true;
}

// Thread worker for server
static DWORD WINAPI StartClient(void* clientArg)
{
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    int ret = 1;
    //char *sendbuf = "this is a test";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    
    int i;
    int readLen;
    int accumSizeSent;
    
    FILE * fi;
    
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    UpdateStatus("Connecting to: %s\r\n", destIP);
    
    FailWithAction( getaddrinfo(destIP, portNo, &hints, &result) != 0 , UpdateStatus("getaddrinfo failed with error: %d\n", iResult), EXIT )

    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        FailWithAction( (ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == INVALID_SOCKET, UpdateStatus("socket failed with error!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )

        // Connect to server.
        if (connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
		{
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    FailWithAction( ConnectSocket == INVALID_SOCKET, UpdateStatus("Unable to connect to server!\n"), EXIT )

    // send signal to create a new directory
    ((unsigned int *)recvbuf)[0] = 0xBEEF0000;
    FailWithAction( send( ConnectSocket, recvbuf, sizeof(unsigned int), 0 ) == SOCKET_ERROR, UpdateStatus("send failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )
    
    if (!IsServerResponseValid(&ConnectSocket, recvbuf))
        return 1;
    
    for (i = 0; i < numFiles*2; i+=2)
    {
        ((unsigned int *)recvbuf)[0] = 0xBEEF0001;
        strcpy(&recvbuf[4], fileList[i]);
        
        UpdateStatus("Sending file: %s\r\n", &recvbuf[4]);
        
        FailWithAction( send( ConnectSocket, recvbuf, sizeof(unsigned int) + strlen(fileList[i]) + 1, 0 ) == SOCKET_ERROR, UpdateStatus("send failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT ) 
       
        if (!IsServerResponseValid(&ConnectSocket, recvbuf))
            return 1;
        
        fi = fopen(fileList[i+1], "rb");
        
        readLen = 1;
        accumSizeSent = 0;
        while (!feof(fi) && readLen > 0)
        {
            readLen = fread(recvbuf, 1, DEFAULT_BUFLEN, fi);

            if (readLen > 0)
            {
                FailWithAction( (iResult = send( ConnectSocket, recvbuf, readLen, 0 )) == SOCKET_ERROR, UpdateStatus("send failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT ) 
                accumSizeSent += iResult;
                //printf("Bytes Sent: %ld\n", accumSizeSent);
            }
            
            if (!IsServerResponseValid(&ConnectSocket, recvbuf))
                return 1;
        }

        UpdateStatus("Total bytes Sent: %d\r\n", accumSizeSent);
        
        ((unsigned int *)recvbuf)[0] = 0xBEEF0002;
        FailWithAction( send( ConnectSocket, recvbuf, sizeof(unsigned int), 0 ) == SOCKET_ERROR, UpdateStatus("send failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT ) 
        
        if (!IsServerResponseValid(&ConnectSocket, recvbuf))
            return 1;
    }

    // signal the end of file
    ((unsigned int *)recvbuf)[0] = 0xDEADBEEF;
    ((unsigned int *)recvbuf)[1] = 0xBEEFFACE;
    FailWithAction( (iResult = send( ConnectSocket, recvbuf, 2*sizeof(unsigned int), 0 )) == SOCKET_ERROR, UpdateStatus("send failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT )
	UpdateStatus("Last Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    FailWithAction( shutdown(ConnectSocket, SD_SEND) == SOCKET_ERROR, UpdateStatus("shutdown failed!\r\n%s\r\n", PrintLastError(WSAGetLastError())), EXIT) 

    ret = 0;
EXIT:
	// cleanup
	closesocket(ConnectSocket);

    // Don't forget to reset it for next transaction
    numFiles = 0;

    EnableWindow(GetDlgItem( dlgHandle, IDC_START ), true);
    
    return ret;
}

// DialogProc

void OpenFileChooserDialog(void)
{
    char buffer[BUFSIZE] = {0};
    OPENFILENAME ofns = {0};
    int i;
    
    char buffall[BUFSIZE*16];

    fileList = GetFileName(&ofns, buffer, &numFiles, "Choose Name:" );
    
    buffall[0] = '\0';
    sprintf(&buffall[strlen(buffall)], "Files Chosen:\r\n");
    for(i = 0; i < numFiles*2; i+=2)
    {
        //sprintf(&buffall[strlen(buffall)], "%s\r\n", fileList[i]);
        sprintf(&buffall[strlen(buffall)], "%s\r\n", fileList[i+1]);
    }
    
    UpdateStatusPanel(buffall);
}

BOOL CALLBACK DialogProc( HWND hwnd, UINT message, WPARAM wp, LPARAM lp ) 
{
    char buffer[BUFSIZE*16];
    HICON hIcon;

    switch ( message ) 
    { 
        case WM_INITDIALOG:
            
            hIcon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MYICON));
            if(hIcon)
            {
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            }

            // init various things
            dlgHandle = hwnd;
            SetWindowText(hwnd, "Welcome!");
            
            CheckDlgButton(hwnd, IDC_RECV, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_SEND, BST_UNCHECKED);
            
            SetWindowText(GetDlgItem( hwnd, IDC_IP_ADDR ), "127.0.0.1");
            SetWindowText(GetDlgItem( hwnd, IDC_PORT ), DEFAULT_PORT);
            
            getLocalAdaptersInfo(buffer);
            SetWindowText(GetDlgItem( hwnd, IDC_LOCAL_IP ), buffer);
            
            // SetFocus doesn't work here but we can set the tab-order in resource editor
            //SetFocus(GetDlgItem( hwnd, IDC_IP_ADDR ));
            
            return TRUE;
            
        case WM_COMMAND: 
        {
            int ctl = LOWORD( wp ); 
            int event = HIWORD( wp );
            if ( ctl == IDCANCEL && event == BN_CLICKED )
            {
                DestroyWindow (hwnd);
                return TRUE;     
            }
            else if ( ctl == IDC_CHOOSE_FILES && event == BN_CLICKED )
            {
                OpenFileChooserDialog();
                return TRUE;     
            }
            else if ( ctl == IDC_START && event == BN_CLICKED )
            {
                EnableWindow(GetDlgItem( hwnd, IDC_START ), false);

                if (IsDlgButtonChecked(hwnd, IDC_RECV))
                {
                    // Don't forget to get the port number
                    GetWindowText(GetDlgItem( hwnd, IDC_PORT ), portNo, GetWindowTextLength( GetDlgItem( hwnd, IDC_PORT ) ) + 1);
                    CreateThread(
                                NULL,                   /* default security attributes.   */
                                0,                      /* use default stack size.        */
                                StartServer,          /* thread function name.          */
                                (void*)NULL,        /* argument to thread function.   */
                                0,                      /* use default creation flags.    */
                                &serverDescriptor);     /* returns the thread identifier. */
                }
                else if (IsDlgButtonChecked(hwnd, IDC_SEND))
                {
                    if (numFiles <= 0)
                    {
                        MessageBox(NULL, "No Files Chosen", "Error", 0);
                        EnableWindow(GetDlgItem( hwnd, IDC_START ), true);
                        return TRUE;
                    }
                    
                    // First get the IP and port numbers
                    GetWindowText(GetDlgItem( hwnd, IDC_IP_ADDR ), destIP, GetWindowTextLength( GetDlgItem( hwnd, IDC_IP_ADDR ) ) + 1);
                    GetWindowText(GetDlgItem( hwnd, IDC_PORT ), portNo, GetWindowTextLength( GetDlgItem( hwnd, IDC_PORT ) ) + 1);

                    CreateThread(
                                NULL,                   /* default security attributes.   */
                                0,                      /* use default stack size.        */
                                StartClient,          /* thread function name.          */
                                (void*)NULL,        /* argument to thread function.   */
                                0,                      /* use default creation flags.    */
                                &clientDescriptor);     /* returns the thread identifier. */
                }
                
                return TRUE;
            }
            
            return FALSE;
        }
        case WM_DESTROY:
            WSACleanup();        
            PostQuitMessage(0);         
            return TRUE;
        case WM_CLOSE:
            WSACleanup();
            DestroyWindow (hwnd);
            return TRUE;     
   }
   return FALSE;
}

void StartDialog(void)
{
    HWND dlg = CreateDialog( GetModuleHandle(0),
                            MAKEINTRESOURCE( IDD_MAIN_SOCKET  ), 
                            0, DialogProc );
    MSG dlgMsg;
    while ( GetMessage( & dlgMsg, 0, 0, 0 ) ) 
    {
        if ( ! IsDialogMessage( dlg, & dlgMsg ) ) 
        {
            TranslateMessage( & dlgMsg );
            DispatchMessage( & dlgMsg );
        }
    }	
}

int main(void) 
{    
    WSAData wsaData;
    int iResult;
    
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) 
    {
		sprintf(msg, "WSAStartup failed with error: %d\n", iResult);
        MessageBox(NULL, msg, "Error", 0);
        return 1;
    }
 
    // setup initial variables
    numFiles = 0;
    statusBuffer = (char *) malloc(BUFSIZE * 1024 * 4);
    statusBuffer[0] = '\0';
    
    StartDialog();
    
    WSACleanup();
    
    return 0;
}


