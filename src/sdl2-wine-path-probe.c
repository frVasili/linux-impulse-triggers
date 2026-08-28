/*
 * Mirror Wine winebus.sys's SDL2 initialization and zero-intensity rumble
 * capability probes. This never requests nonzero vibration.
 */

#include <SDL2/SDL.h>

#include <stdio.h>

static const char *safe(const char *text)
{
    return text ? text : "(null)";
}

int main(void)
{
    SDL_version linked;
    Uint32 deadline;
    int count;

    SDL_GetVersion(&linked);
    printf("SDL2 compile version: %u.%u.%u\n",
           SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    printf("SDL2 linked version:  %u.%u.%u\n",
           linked.major, linked.minor, linked.patch);
    printf("SDL revision:         %s\n", safe(SDL_GetRevision()));

    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", safe(SDL_GetError()));
        return 1;
    }

    deadline = SDL_GetTicks() + 2500;
    while (SDL_GetTicks() < deadline) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
        }
        SDL_Delay(10);
    }
    SDL_JoystickUpdate();

    count = SDL_NumJoysticks();
    printf("SDL joystick count:   %d\n", count);
    for (int i = 0; i < count; ++i) {
        SDL_Joystick *joystick;
        SDL_GameController *controller = NULL;
        int ordinary_result;
        int trigger_result;

        printf("\njoystick[%d]\n", i);
        printf("  device name:            %s\n", safe(SDL_JoystickNameForIndex(i)));
        printf("  device path:            %s\n", safe(SDL_JoystickPathForIndex(i)));
        printf("  SDL_IsGameController:   %s\n",
               SDL_IsGameController(i) ? "true" : "false");

        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            joystick = controller ? SDL_GameControllerGetJoystick(controller) : NULL;
        } else {
            joystick = SDL_JoystickOpen(i);
        }
        if (!joystick) {
            printf("  open:                    failure: %s\n", safe(SDL_GetError()));
            SDL_ClearError();
            continue;
        }

        printf("  open name:               %s\n", safe(SDL_JoystickName(joystick)));
        printf("  open path:               %s\n", safe(SDL_JoystickPath(joystick)));
        printf("  VID:PID:                 %04x:%04x\n",
               SDL_JoystickGetVendor(joystick), SDL_JoystickGetProduct(joystick));
        printf("  axes/buttons/hats:       %d/%d/%d\n",
               SDL_JoystickNumAxes(joystick), SDL_JoystickNumButtons(joystick),
               SDL_JoystickNumHats(joystick));

        ordinary_result = SDL_JoystickRumble(joystick, 0, 0, 0);
        printf("  ordinary zero call:      %s%s%s\n",
               ordinary_result == 0 ? "success" : "failure",
               ordinary_result == 0 ? "" : ": ",
               ordinary_result == 0 ? "" : safe(SDL_GetError()));
        SDL_ClearError();

        trigger_result = SDL_JoystickRumbleTriggers(joystick, 0, 0, 0);
        printf("  trigger zero call:       %s%s%s\n",
               trigger_result == 0 ? "success" : "failure",
               trigger_result == 0 ? "" : ": ",
               trigger_result == 0 ? "" : safe(SDL_GetError()));
        SDL_ClearError();

        if (controller) {
            SDL_GameControllerClose(controller);
        } else {
            SDL_JoystickClose(joystick);
        }
    }

    SDL_Quit();
    return 0;
}
