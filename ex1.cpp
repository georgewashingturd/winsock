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
    char msg[BUFSIZE];

    buffer[0] = '\0';

    ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);

    pAdapterInfo = (IP_ADAPTER_INFO *) malloc(sizeof (IP_ADAPTER_INFO));

    if (pAdapterInfo == NULL)
    {
        UpdateStatusPanel("Error allocating memory needed to call GetAdaptersinfo()\r\n");
        return 1;
    }
    else
        UpdateStatusPanel("Memory allocation for GetAdaptersinfo() call is OK!\r\n");

    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
    {
        UpdateStatusPanel("Not enough buffer! Re-allocating...\r\n");
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen);
        if (pAdapterInfo == NULL)
        {
            UpdateStatusPanel("Error allocating memory needed to call GetAdaptersinfo()\r\n");
            return 1;
        }
        else
            UpdateStatusPanel("Memory allocation for GetAdaptersinfo() 2nd call is OK!\r\n");
    }
    else
        UpdateStatusPanel("Buffer for GetAdaptersInfo() is OK!\r\n");

 

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
        sprintf(msg, "GetAdaptersInfo failed with error: %d\r\n", dwRetVal);
        UpdateStatusPanel(msg);
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

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    char workbuf[DEFAULT_BUFLEN*2];
    int recvbuflen = DEFAULT_BUFLEN;
    
    char folderName[128];
    char msg[BUFSIZE];
    
    // Winsock should have been initialized

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    UpdateStatusPanel("Starting Server ...\r\n");

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, portNo, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR)
    {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        return 1;
    }

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET)
    {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        return 1;
    }

    // No longer need server socket
    closesocket(ListenSocket);

    // Receive until the peer shuts down the connection
    FILE * fo;
    int accumSizeRecv;
    do
    {        
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
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
                sprintf(msg, "Creating folder: %s\r\n", folderName);
                UpdateStatusPanel(msg);
                
                CreateDirectory(folderName, NULL);
            }
            // 0xBEEF0001 is indicator that the following data is the file name
            else if (((unsigned int *) recvbuf)[0] == 0xBEEF0001)
            {
                char fullPath[BUFSIZE];
                
                sprintf(fullPath, "%s\\%s", folderName, &recvbuf[4]);
                
                sprintf(msg, "Receiving file name: %s\r\n", &recvbuf[4]);
                UpdateStatusPanel(msg);
                
                fo = fopen(fullPath, "wb");
                if (fo == NULL)
                {
                    closesocket(ClientSocket);
                    return 1;
                }
                accumSizeRecv = 0;
            }
            // 0xBEEF0002 is indicator of end of file
            else if (((unsigned int *) recvbuf)[0] == 0xBEEF0002)
            {
                fclose(fo);
                sprintf(msg, "recv complete: %d bytes\r\n", accumSizeRecv);
                UpdateStatusPanel(msg);
            }
            else if (((unsigned int *) recvbuf)[0] == 0xDEADBEEF && ((unsigned int *) recvbuf)[1] == 0xBEEFFACE)
            {
                fclose(fo);

                sprintf(msg, "Transaction complete, closing connection ...\r\n", accumSizeRecv);
                UpdateStatusPanel(msg);
                
                closesocket(ClientSocket);
                return 1;
            }
            else
            {
                accumSizeRecv += iResult;
                //printf("Bytes received: %d\n", accumSizeRecv);

                int writeLen;
                writeLen = fwrite(recvbuf, 1, iResult, fo);

                if (writeLen != iResult)
                {
                    return 1;
                }
            }
            
            // send valid response after every receive
            ((unsigned int *)recvbuf)[0] = 0xBEEFFACE;
            iResult = send( ClientSocket, recvbuf, sizeof(unsigned int), 0 );
            if (iResult == SOCKET_ERROR) 
            {
                printf("send response failed: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                return 1;
            }
        }
        else if (iResult == 0)
            printf("Connection closing...\n");
        else
        {
            printf("recv failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            return 1;
        }

    } while (iResult > 0);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR)
    {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    

    return 0;
}

bool IsServerResponseValid(SOCKET * ConnectSocket, char recvbuf[])
{
    int iResult;
    
    ((unsigned int *)recvbuf)[0] = 0x5A5A5A5A;
    iResult = recv(*ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
    if (((unsigned int *)recvbuf)[0] != 0xBEEFFACE)
    {
        printf("Server response error %d\n", WSAGetLastError());
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
    int rc;
    //char *sendbuf = "this is a test";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    
    int i;
    int readLen;
    int accumSizeSent;
    
    FILE * fi;
    
    char msg[BUFSIZE];

    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    sprintf(msg, "Connecting to: %s\r\n", destIP);
    UpdateStatusPanel(msg);
    
    iResult = getaddrinfo(destIP, portNo, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, 
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            return 1;
        }

        // Connect to server.
        iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET)
    {
        printf("Unable to connect to server!\n");
        return 1;
    }

    // send signal to create a new directory
    ((unsigned int *)recvbuf)[0] = 0xBEEF0000;
    iResult = send( ConnectSocket, recvbuf, sizeof(unsigned int), 0 );
    if (iResult == SOCKET_ERROR) 
    {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        return 1;
    }
    
    if (!IsServerResponseValid(&ConnectSocket, recvbuf))
        return 1;
    
    for (i = 0; i < numFiles*2; i+=2)
    {
        ((unsigned int *)recvbuf)[0] = 0xBEEF0001;
        strcpy(&recvbuf[4], fileList[i]);
        
        sprintf(msg, "Sending file: %s\r\n", &recvbuf[4]);
        UpdateStatusPanel(msg);
        
        iResult = send( ConnectSocket, recvbuf, sizeof(unsigned int) + strlen(fileList[i]) + 1, 0 );
        if (iResult == SOCKET_ERROR) 
        {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            return 1;
        }
        if (!IsServerResponseValid(&ConnectSocket, recvbuf))
            return 1;
        
        fi = fopen(fileList[i+1], "rb");
        
        readLen = 1;
        accumSizeSent = 0;
        while (!feof(fi) && readLen > 0)
        {
            readLen = fread(recvbuf,1,DEFAULT_BUFLEN,fi);

            if (readLen > 0)
            {
                iResult = send( ConnectSocket, recvbuf, readLen, 0 );
                if (iResult == SOCKET_ERROR) 
                {
                    printf("send failed with error: %d\n", WSAGetLastError());
                    closesocket(ConnectSocket);
                    return 1;
                }
                
                accumSizeSent += iResult;
                //printf("Bytes Sent: %ld\n", accumSizeSent);
            }
            
            if (!IsServerResponseValid(&ConnectSocket, recvbuf))
                return 1;
        }
        
        sprintf(msg, "Total bytes Sent: %ld\r\n", accumSizeSent);
        UpdateStatusPanel(msg);
        
        ((unsigned int *)recvbuf)[0] = 0xBEEF0002;
        iResult = send( ConnectSocket, recvbuf, sizeof(unsigned int), 0 );
        if (iResult == SOCKET_ERROR) 
        {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(ConnectSocket);
            return 1;
        }
        
        if (!IsServerResponseValid(&ConnectSocket, recvbuf))
            return 1;
    }

    // signal the end of file
    ((unsigned int *)recvbuf)[0] = 0xDEADBEEF;
    ((unsigned int *)recvbuf)[1] = 0xBEEFFACE;
    iResult = send( ConnectSocket, recvbuf, 2*sizeof(unsigned int), 0 );
    printf("Last Bytes Sent: %ld\n", iResult);
    if (iResult == SOCKET_ERROR)
    {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        return 1;
    }

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    
    if (iResult == SOCKET_ERROR) 
    {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        return 1;        
    }
    
    // cleanup
    closesocket(ConnectSocket);
    
    // Don't forget to reset it for next transaction
    numFiles = 0;

    return 0;
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

    switch ( message ) 
    { 
        case WM_INITDIALOG:            
            // init various things
            dlgHandle = hwnd;
            SetWindowText(hwnd, "Welcome!");
            
            CheckDlgButton(hwnd, IDC_RECV, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_SEND, BST_UNCHECKED);
            
            SetWindowText(GetDlgItem( hwnd, IDC_IP_ADDR ), "127.0.0.1");
            SetWindowText(GetDlgItem( hwnd, IDC_PORT ), DEFAULT_PORT);
            
            getLocalAdaptersInfo(buffer);
            SetWindowText(GetDlgItem( hwnd, IDC_LOCAL_IP ), buffer);
            
            // SetFocus doesn't work here
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
    MSG msg;
    while ( GetMessage( & msg, 0, 0, 0 ) ) 
    {
        if ( ! IsDialogMessage( dlg, & msg ) ) 
        {
            TranslateMessage( & msg );
            DispatchMessage( & msg );
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
        printf("WSAStartup failed with error: %d\n", iResult);
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


