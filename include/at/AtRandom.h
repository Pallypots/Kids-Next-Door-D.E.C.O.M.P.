// The game's table of random words, 0x800 of them at 0x002AF0B0, with a cursor at 0x002C60C0 that every draw moves on and wraps.
//
// In the original this is plain data in .data: nothing writes it, and code only reads it. Each draw is written out inline, in 0x0001E8D0, 0x0004C3B0, 0x0004C8B0 and others. The contents are not arbitrary, though: all 2048 words are
// what the Microsoft C library's rand() gives, masked to 11 bits, from one seed. So here they are computed, not stored. The oracle compares every word with the owner's XBE. Whoever generated them did so when the game was built; only the low 27
// bits of the seed can show in the words, so the true seed is one of 32, and how many calls came before the first is not known.
#pragma once

extern unsigned int g_atRandomCursor;  // 0x002C60C0

unsigned short AtRandomWord(unsigned int index);  // index below 0x800
unsigned short AtNextRandomWord();                // inline in the original: on one, wrapped, and the word found there
