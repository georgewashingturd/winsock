#include <windows.h>
#include <stdio.h>
#include <string.h>

void TypeB(void)
{
    INPUT input;

    ZeroMemory(&input, sizeof(INPUT));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_BACK;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
    
    ZeroMemory(&input, sizeof(INPUT));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_BACK;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}


void Type(char * chr)
{
    INPUT input;
    unsigned int length = strlen(chr);
    
    for (unsigned int j = 0; j < length; j++)
    {
        
        SHORT virtKey = VkKeyScan((TCHAR) chr[j]);
        
        if (HIBYTE(virtKey) == 1)
        {
            ZeroMemory(&input, sizeof(INPUT));
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = VK_SHIFT;
            input.ki.dwFlags = 0;
            SendInput(1, &input, sizeof(INPUT));
        }
        
        ZeroMemory(&input, sizeof(INPUT));
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = LOBYTE(virtKey);
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(INPUT));
        
        ZeroMemory(&input, sizeof(INPUT));
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = LOBYTE(virtKey);
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        
        if (HIBYTE(virtKey) == 1)
        {
            ZeroMemory(&input, sizeof(INPUT));
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = VK_SHIFT;
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
        }
    }
}

int main(int argc, char * argv[])
{
    // This structure will be used to create the keyboard
    // input event.
    INPUT ip;
 
    // Pause for 5 seconds.
    Sleep(2000);
 
    Type((char *)"!Ab");
    TypeB();
 
    // Exit normally
    return 0;
}