windres  -J rc -O coff -i winsock.rc -o winsock.res 
g++ ex1.cpp winsock.res -o ex1.exe -lcomdlg32 -lws2_32 -liphlpapi -lcomctl32 -static
