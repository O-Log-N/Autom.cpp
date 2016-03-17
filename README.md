# Autom.cpp

The idea of Autom.cpp is to create an environment which is more or less similar to Node.js, with the following improvements:

- fully deterministic implementation
    - it will allow us to implement several all-important goodies, such as replay-based testing and production post-mortem, as described in http://ithare.com/chapter-vc-modular-architecture-client-side-on-debugging-distributed-systems-deterministic-logic-and-finite-state-machines/
- improved handling of non-blocking IO
    - "non-blocking futures" along the lines of http://ithare.com/asynchronous-processing-for-finite-state-machines-actors-from-plain-events-to-futures-with-oo-and-lambda-call-pyramids-in-between/
    - "implicit co-routines" along the lines which have never been described yet ;-)
- integrated IDL (Hare-IDL, to be exact)

BTW, "Autom" is a short for "Automaton", as our Autom is a deterministic finite automaton. 
