/*
 * Generic SDL3 four-motor controller diagnostic.
 *
 * Build:
 *   cc -std=c11 -Wall -Wextra -Wpedantic -O2 \
 *      sdl3-four-motor-test.c -o sdl3-four-motor-test \
 *      $(pkg-config --cflags --libs sdl3)
 *
 * Probe only (never sends nonzero rumble):
 *   ./sdl3-four-motor-test --probe
 *
 * Interactive motor sequence for the displayed joystick index:
 *   ./sdl3-four-motor-test --rumble 0
 */

#include <SDL3/SDL.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *nonnull_string(const char *value)
{
    return value ? value : "(null)";
}

static void print_result(const char *operation, bool result)
{
    printf("result: %-31s %s", operation, result ? "success" : "failure");
    if (!result) {
        printf("; SDL error: %s", nonnull_string(SDL_GetError()));
    }
    putchar('\n');
    fflush(stdout);
    SDL_ClearError();
}

static void wait_with_events(Uint32 milliseconds)
{
    const Uint64 deadline = SDL_GetTicks() + milliseconds;
    SDL_Event event;

    while (SDL_GetTicks() < deadline) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                return;
            }
        }
        SDL_Delay(10);
    }
}

static void wait_for_enter(const char *label)
{
    char line[8];

    printf("\nREADY: %s\nPress Enter to run this pattern...", label);
    fflush(stdout);
    (void)fgets(line, sizeof(line), stdin);
}

static void stop_all(SDL_Joystick *joystick)
{
    print_result("ordinary rumble off", SDL_RumbleJoystick(joystick, 0, 0, 0));
    print_result("trigger rumble off", SDL_RumbleJoystickTriggers(joystick, 0, 0, 0));
    wait_with_events(250);
}

static void ordinary_pattern(SDL_Joystick *joystick, const char *label,
                             Uint16 low, Uint16 high)
{
    char operation[128];

    wait_for_enter(label);
    printf("\nPATTERN: %s for 2000 ms\n", label);
    snprintf(operation, sizeof(operation), "ordinary low=%u high=%u", low, high);
    print_result(operation, SDL_RumbleJoystick(joystick, low, high, 2000));
    wait_with_events(2250);
    stop_all(joystick);
}

static void trigger_pattern(SDL_Joystick *joystick, const char *label,
                            Uint16 left, Uint16 right)
{
    char operation[128];

    wait_for_enter(label);
    printf("\nPATTERN: %s for 2000 ms\n", label);
    snprintf(operation, sizeof(operation), "trigger left=%u right=%u", left, right);
    print_result(operation, SDL_RumbleJoystickTriggers(joystick, left, right, 2000));
    wait_with_events(2250);
    stop_all(joystick);
}

static void print_joystick(SDL_Joystick *joystick, int index)
{
    char guid[64];
    SDL_PropertiesID properties = SDL_GetJoystickProperties(joystick);

    SDL_GUIDToString(SDL_GetJoystickGUID(joystick), guid, sizeof(guid));
    printf("\njoystick[%d]\n", index);
    printf("  name:                    %s\n", nonnull_string(SDL_GetJoystickName(joystick)));
    printf("  path/backend clue:       %s\n", nonnull_string(SDL_GetJoystickPath(joystick)));
    printf("  serial:                  %s\n", nonnull_string(SDL_GetJoystickSerial(joystick)));
    printf("  VID:PID:                 %04x:%04x\n",
           SDL_GetJoystickVendor(joystick), SDL_GetJoystickProduct(joystick));
    printf("  product version:         %04x\n", SDL_GetJoystickProductVersion(joystick));
    printf("  firmware version:        %04x\n", SDL_GetJoystickFirmwareVersion(joystick));
    printf("  GUID:                    %s\n", guid);
    printf("  type enum:               %d\n", (int)SDL_GetJoystickType(joystick));
    printf("  axes/buttons/hats:       %d/%d/%d\n",
           SDL_GetNumJoystickAxes(joystick), SDL_GetNumJoystickButtons(joystick),
           SDL_GetNumJoystickHats(joystick));
    printf("  ordinary rumble cap:     %s\n",
           SDL_GetBooleanProperty(properties, SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, false)
               ? "true" : "false");
    printf("  trigger rumble cap:      %s\n",
           SDL_GetBooleanProperty(properties,
                                  SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN, false)
               ? "true" : "false");
}

static bool parse_index(const char *text, int *index)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value < 0 || value > INT_MAX) {
        return false;
    }
    *index = (int)value;
    return true;
}

static void usage(const char *program)
{
    fprintf(stderr, "usage: %s --probe | --rumble JOYSTICK_INDEX\n", program);
}

int main(int argc, char **argv)
{
    bool do_rumble = false;
    int requested_index = -1;
    int count = 0;
    SDL_JoystickID *ids = NULL;
    SDL_Joystick *target = NULL;

    if (argc == 2 && strcmp(argv[1], "--probe") == 0) {
        do_rumble = false;
    } else if (argc == 3 && strcmp(argv[1], "--rumble") == 0 &&
               parse_index(argv[2], &requested_index)) {
        do_rumble = true;
    } else {
        usage(argv[0]);
        return 2;
    }

    SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_VERBOSE);
    printf("SDL compile version: %d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
           SDL_MICRO_VERSION);
    printf("SDL runtime version: %d\n", SDL_GetVersion());
    printf("SDL runtime revision: %s\n", nonnull_string(SDL_GetRevision()));

    if (!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init failed: %s\n", nonnull_string(SDL_GetError()));
        return 1;
    }

    /* Xbox GIP may wait up to two seconds for its protocol hello. */
    printf("Waiting 2500 ms for asynchronous controller initialization...\n");
    wait_with_events(2500);
    SDL_UpdateJoysticks();

    ids = SDL_GetJoysticks(&count);
    if (!ids) {
        fprintf(stderr, "SDL_GetJoysticks failed: %s\n", nonnull_string(SDL_GetError()));
        SDL_Quit();
        return 1;
    }
    printf("SDL joystick count: %d\n", count);

    for (int i = 0; i < count; ++i) {
        SDL_Joystick *joystick = SDL_OpenJoystick(ids[i]);
        if (!joystick) {
            fprintf(stderr, "SDL_OpenJoystick(id=%" PRIu32 ") failed: %s\n",
                    ids[i], nonnull_string(SDL_GetError()));
            continue;
        }
        print_joystick(joystick, i);
        if (do_rumble && i == requested_index) {
            target = joystick;
        } else {
            SDL_CloseJoystick(joystick);
        }
    }
    SDL_free(ids);

    if (do_rumble && !target) {
        fprintf(stderr, "Joystick index %d could not be opened.\n", requested_index);
        SDL_Quit();
        return 1;
    }

    if (target) {
        printf("\nStarting five separated motor patterns. Hold the controller only; "
               "do not rest it on a resonant surface.\n");
        ordinary_pattern(target, "ordinary LOW-FREQUENCY motor 100%, high 0%", 65535, 0);
        ordinary_pattern(target, "ordinary HIGH-FREQUENCY motor 100%, low 0%", 0, 65535);
        trigger_pattern(target, "LEFT TRIGGER 100%, right trigger 0%", 65535, 0);
        trigger_pattern(target, "RIGHT TRIGGER 100%, left trigger 0%", 0, 65535);
        trigger_pattern(target, "BOTH TRIGGERS 50%", 32768, 32768);
        printf("\nPATTERN: all motors off\n");
        stop_all(target);
        SDL_CloseJoystick(target);
    }

    SDL_Quit();
    return 0;
}
