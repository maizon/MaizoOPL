/* Independent In-Game PRO IGR routing.
 *
 * With the MAIZO overlay installed, a game must never exit solely
 * because its framebuffer cannot be discovered or the config key was
 * missing. Only an explicit menu action may request RESET/POWEROFF.
 * Kept pure so host tests can enforce the contract without PS2 SDK.
 */
#ifndef MAIZO_IGR_ROUTE_H
#define MAIZO_IGR_ROUTE_H
enum {
    MAIZO_IGR_RESUME = 0,
    MAIZO_IGR_EXIT = 1,
    MAIZO_IGR_POWEROFF = 2,
    MAIZO_IGR_UNAVAILABLE = 3
};
static inline int maizoIgrSafeChoice(int menu_enabled, int choice)
{
    if (menu_enabled &&
        (choice == MAIZO_IGR_EXIT || choice == MAIZO_IGR_POWEROFF))
        return choice;
    return MAIZO_IGR_RESUME;
}
#endif
