/*! \file */

/* Copyright (C) 2010 Atsushi Togo
 All rights reserved.

 This file is part of spglib. https://github.com/atztogo/spglib

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

 * Neither the name of the phonopy project nor the names of its
   contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE. */

#ifndef __spg_database_H__
#define __spg_database_H__

#include<iostream>
#include<string>
#include "symmetry.h"

/*! \brief A Bravais letter indicating a centering of a lattice whose conventional cell is centred.

When the unit cell does not reflec the symmetry of the lattice, it is usual to
refer to a 'conventional' crystallographic basis, (aₛ bₛ cₛ), instead of a
primitive basis, (aₚ bₚ cₚ).
Such a conventional basis has "extra" lattice points added at the centre of the
unit cell, the centre of a face, or the centre of three faces.
The "extra" nodes in the conventional basis are displaced from the origin of the
unit cell by 'centring vectors'. As with any space-spanning basis, any
whole-number linear combination of the conventional basis vectors is a lattice
point but in addition there exist linear combinations xaₛ+ybₛ+zcₛ with at least
two fractional coefficients (x,y,z) that are lattice points as well.

Each conventional basis is ascribed a Bravais letter, which forms part of the
Hermann-Mauguin symbol of a space group.
A subset of the 10 possible Bravais letters is used herein:

| Bravai letter | Centring | Centring vectors |
| --- | --- | --- |
| P | primitve | 0 |
| A | A-face centred | ½bₛ+½cₛ |
| B | B-face centred | ½cₛ+½aₛ |
| C | C-face centred | ½aₛ+½bₛ |
| I | body centred (*Innenzentriert*) | ½aₛ+½bₛ+½cₛ |
| F | all-face centred | ½bₛ+½cₛ, ½cₛ+½aₛ, ½aₛ+½bₛ |
| R | rhombohedrally centred (hexagonal axes) | ⅔aₛ+⅓bₛ+⅓cₛ, ⅓aₛ+⅓bₛ+⅔c |

For further details, see http://reference.iucr.org/dictionary/Centred_lattice
*/
enum BravaisLetter {P, A, B, C, I, F, R};

typedef struct {
  int           number;
  char          schoenflies[7];
  char          hall_symbol[17];
  char          international[32];
  char          international_full[20];
  char          international_short[11];
  char          choice[6];
  BravaisLetter bravais;
  int           pointgroup_number;
} Spacegroup;

bool hall_number_ok(const int hall_number);
Symmetry get_spacegroup_symmetry_operations(const int hall_number);
Spacegroup get_spacegroup(const int hall_number);

int international_number_to_hall_number(const int number);
int international_string_to_hall_number(const std::string& itname);

#endif
