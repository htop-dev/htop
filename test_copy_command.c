#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Test the copy command logic without htop dependencies
void test_copyCommand(const char* command) {
   if (!command) return;
   
   char copyCmd[1024];
   bool success = false;
   
   printf("Testing copy for command: %s\n", command);
   
#ifdef __APPLE__
   snprintf(copyCmd, sizeof(copyCmd), "printf '%%s' '%s' | pbcopy", command);
   printf("macOS command: %s\n", copyCmd);
   success = (system(copyCmd) == 0);
#elif defined(__linux__) || defined(__unix__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
   snprintf(copyCmd, sizeof(copyCmd), "printf '%%s' '%s' | xclip -selection clipboard 2>/dev/null", command);
   printf("Linux command (xclip): %s\n", copyCmd);
   if (system(copyCmd) != 0) {
      snprintf(copyCmd, sizeof(copyCmd), "printf '%%s' '%s' | xsel --clipboard 2>/dev/null", command);
      printf("Linux command (xsel): %s\n", copyCmd);
      success = (system(copyCmd) == 0);
   } else {
      success = true;
   }
#endif
   
   if (success) {
      printf("✅ Command copied to clipboard\n");
   } else {
      printf("❌ Copy failed - clipboard not available\n");
   }
   printf("\n");
}

int main() {
   printf("Testing copy command functionality...\n\n");
   
   // Test various command types
   test_copyCommand("/usr/bin/vim /etc/hosts");
   test_copyCommand("python3 -m http.server 8000");
   test_copyCommand("htop");
   test_copyCommand("ls -la /home/user");
   
   return 0;
}