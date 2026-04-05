# VCU Core

This is a purely C++ library that will be used not only be our VCU,
but by our transient sim. That way, we can test VCU software on a model
of the car with rapid iteration.

## Block Diagram

The functionality is organized into functional blocks, kind of like
a Matlab Simulink model. Blocks must not reference other blocks; all info
that enters and exits a block must go through the designated inputs and
outputs. This allows us to test blocks independently and facilitates
collaboration.

(block diagram goes here)
