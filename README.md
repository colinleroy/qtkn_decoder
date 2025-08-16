# Minimal QTKN (Quicktake 150 decoder)

This repository aims to be a test bed to understand and unfuck the Quicktake 150 picture decoder, for which the only algorithm I know of is the one from dcraw.

## The difficulties

This algorithm (in qtkn-decoder.c) is very obscure. It is absolutely undocumented and full of magic.
The one thing I figured so far is that the main `for (c = 0; c < 3; c++)` loop iterations decodes one color plane from RGB.

No idea about the `for (r = 0; r <= !c; r++)` subloop.

## The goal

The goal is to write an efficient algorithm to use in the Quicktake for Apple II project, as this one is very long, even when limited to monochrome decoding. Reaching good performance would mean getting rid of all possible 16-bit maths, especially multiplications and divisions, but I fear this will be hard.
