/* Dicto
 * templnode_.h
 *
 * Copyright (C) 2015 Gregory P. Hedger
 * greg@hedgersoftware.com
 * 30329 112th Pl. SE
 * Auburn, WA 98092
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Also add information on how to contact you by electronic and paper mail.
 *
 */

#pragma once

#include <assert.h>

template <typename kt_, class nc_> class TemplNode {
 public:
  // constuctor
  TemplNode() {
      assert(0);  // Don't call
  }
  TemplNode( kt_ key ) {
      clear();
  }
  // destructor
  ~TemplNode() {}

  // getters/setters
  void SetKey( kt_ key ) { key_ = key; }
  kt_ GetKey() { return key_; }
  void SetParent( nc_ *pParent ) { parent_ = pParent; }
  nc_ * GetParent() { return parent_; }
  void SetLeft( nc_ *pNode ) { l_ = pNode; }
  nc_ * GetLeft() { return l_; }
  void SetRight( nc_ *pNode ) { r_ = pNode; }
  nc_ * GetRight() { return r_; }
  void SetCenter( nc_ *pNode ) { c_ = pNode; }
  nc_ * GetCenter() { return c_; }
  void SetTerminator() { terminator_ = 1; }
  bool GetTerminator() { return terminator_ ? true : false; }
  void SetUpper() { upper_ = 1; }
  bool GetUpper() { return upper_ ? true : false; }

  // clear out node.
  void clear() {
    parent_ = l_ = c_ = r_ = nullptr;
    terminator_ = 0;
    upper_ = 0;
  }

  // member variables
  nc_ *       parent_;    // parent
  nc_ *       l_;         // left (lo kid/less than)
  nc_ *       c_;         // center (equal kid)
  nc_ *       r_;         // right (hi kid/greater than)
  kt_         key_;       // key
  unsigned    terminator_: 1;
  unsigned    upper_: 1;
};
