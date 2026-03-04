#include "DragReductionSystem.h"

/**
 * This system decides whether we should angle our rear wing down (high downforce, but draggier) or
 * more flat (less drag).
 *
 * Angling the rear wing down and producing more downforce means the tires get more friction and the car can
 * turn without drifting as much.
 *
 * Making the rear wing flat by activating the "drag reduction system" or DRS means you exchange that extra friction
 * for having less drag and more speed.
 *
 * You typically want to enable DRS when you're going through a straight part of the track so you can reach a higher
 * top speed, and you disable DRS during turns to get high downforce. With that said, this system needs to decide when
 * to enable or disable DRS based on inputs like the steering wheel angle and the vehicle speed. For example, if the
 * steering wheel is in the middle, the driver is hopefully going straight. If the driver is going straight, we should
 * enable DRS.
*/

void DragReductionSystem::evaluate(VcuParameters *params, DragReductionSystemInput *input, DragReductionSystemOutput *output,
                              float deltaTime) {
    // TODO implement
    output->enable = false;
}
