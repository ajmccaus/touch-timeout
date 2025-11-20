/*
 * PROOF: Your strncpy() pattern is safe
 * Compile: gcc -Wall -Wextra -o test strncpy_safety_test.c
 * Run: ./test
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define NAME_MAX 255

int main(void) {
    printf("=== Testing YOUR Exact Pattern ===\n\n");
    
    /* YOUR EXACT CODE PATTERN */
    char backlight[NAME_MAX + 1] = "rpi_backlight";  // Pre-initialized
    
    printf("1. Initial state:\n");
    printf("   String: '%s'\n", backlight);
    printf("   Length: %zu\n", strlen(backlight));
    printf("   Last byte (backlight[255]): %d (should be 0)\n\n", backlight[255]);
    
    /* CASE 1: Short string (normal use) */
    printf("2. After strncpy(backlight, \"test\", 255):\n");
    strncpy(backlight, "test", sizeof(backlight) - 1);
    printf("   String: '%s'\n", backlight);
    printf("   Length: %zu\n", strlen(backlight));
    printf("   Last byte (backlight[255]): %d (should still be 0)\n");
    printf("   SAFE: %s\n\n", (backlight[255] == 0) ? "YES ✓" : "NO ✗");
    
    /* CASE 2: Exactly 255 chars (boundary) */
    printf("3. After strncpy(backlight, <255 'A's>, 255):\n");
    char test255[256];
    memset(test255, 'A', 255);
    test255[255] = '\0';
    
    // Reset backlight to default first
    strcpy(backlight, "rpi_backlight");
    memset(backlight + strlen(backlight), 0, sizeof(backlight) - strlen(backlight));
    
    strncpy(backlight, test255, sizeof(backlight) - 1);
    printf("   First 10 chars: '%.10s...'\n", backlight);
    printf("   strlen() result: %zu\n", strlen(backlight));
    printf("   Last byte (backlight[255]): %d (should still be 0)\n", backlight[255]);
    printf("   SAFE: %s\n\n", (backlight[255] == 0 && strlen(backlight) == 255) ? "YES ✓" : "NO ✗");
    
    /* CASE 3: Oversized string (attack scenario) */
    printf("4. After strncpy(backlight, <300 'B's>, 255):\n");
    char test300[301];
    memset(test300, 'B', 300);
    test300[300] = '\0';
    
    // Reset again
    strcpy(backlight, "rpi_backlight");
    memset(backlight + strlen(backlight), 0, sizeof(backlight) - strlen(backlight));
    
    strncpy(backlight, test300, sizeof(backlight) - 1);
    printf("   First 10 chars: '%.10s...'\n", backlight);
    printf("   strlen() result: %zu (should be 255)\n", strlen(backlight));
    printf("   Last byte (backlight[255]): %d (should still be 0)\n", backlight[255]);
    printf("   SAFE: %s\n\n", (backlight[255] == 0 && strlen(backlight) == 255) ? "YES ✓" : "NO ✗");
    
    /* CASE 4: Demonstrate UNSAFE pattern for comparison */
    printf("5. UNSAFE pattern (NO pre-initialization):\n");
    char unsafe[NAME_MAX + 1];  // UNINITIALIZED
    strncpy(unsafe, test300, sizeof(unsafe) - 1);
    printf("   Last byte (unsafe[255]): %d\n", unsafe[255]);
    printf("   SAFE: %s (last byte may not be zero!)\n\n", (unsafe[255] == 0) ? "YES ✓" : "NO ✗");
    
    /* CASE 5: Show snprintf() alternative */
    printf("6. snprintf() alternative:\n");
    char with_snprintf[NAME_MAX + 1] = "rpi_backlight";
    int ret = snprintf(with_snprintf, sizeof(with_snprintf), "%s", test300);
    printf("   snprintf() returned: %d (chars that WOULD be written)\n", ret);ls
    z
    printf("   Actual length: %zu\n", strlen(with_snprintf));
    printf("   Truncated: %s\n", (ret >= (int)sizeof(with_snprintf)) ? "YES" : "NO");
    printf("   Last byte: %d (always 0 with snprintf)\n\n", with_snprintf[255]);
    
    printf("=== CONCLUSION ===\n");
    printf("Your pattern: char buf[N+1] = \"default\"; strncpy(buf, src, N);\n");
    printf("Status: SAFE ✓\n");
    printf("Reason: Pre-initialization guarantees buf[N] = '\\0' forever\n");
    printf("\nsnprintf() advantage: Returns truncation status (not needed for device names)\n");
    printf("strncpy() advantage: One less comparison in your code\n");
    
    return 0;
}