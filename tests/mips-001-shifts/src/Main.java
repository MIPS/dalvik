/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * shift operations
 */
public class Main {

    // Arithmetic shift-left the slow way
    // This code is more likely to get jit'ed than the version below
    public static long gashl(long v, int shift) {
	long r = 0;
	long mask;
	shift &= 63;
	mask = 1;
	for (int i = 0; i < 64-shift; i++) {
	    mask = 1;
	    mask <<= i;
	    r |= ((v & mask) << shift);
	}
	return r;
    }

    // Arithmetic shift-left the sane way
    public static long ashl(long v, int shift) {
	return v << shift;
    }

    // Arithmetic shift-right the slow way
    // This code is more likely to get jit'ed than the version below
    public static long gashr(long v, int shift) {
	long r = 0;
	long mask;
	shift &= 63;
	for (int i = 0; i < 64; i++) {
	    mask = 1;
	    mask <<= i;
	    r = r | ((v & mask) >> shift);
	}
	return r;
    }

    // Arithmetic shift-right the sane way
    public static long ashr(long v, int shift) {
	return v >> shift;
    }

    // Unsigned shift-right the slow way
    // This code is more likely to get jit'ed than the version below
    public static long gushr(long v, int shift) {
	long r = 0;
	long mask;
	shift &= 63;
	for (int i = 0; i < 64; i++) {
	    mask = 1;
	    mask <<= i;
	    r = r | ((v & mask) >>> shift);
	}
	return r;
    }

    // Unsigned shift-right the sane way
    public static long ushr(long v, int shift) {
	return v >>> shift;
    }

    public static String toHex(long v) {
	return String.format("%x", v);
    }

    public static void checkone(long v, String op, int shift, long gv, long av)
    {
	if (gv != av)
	    System.out.println(toHex(v)+op+shift+", expected="+toHex(gv)+" got="+toHex(av));
    }

    public static void checkall( int shift) {

	for (int i = 0; i < 64; i++) {
	    long v = 1;
	    v <<= i;
	    checkone(v, "<<",  shift, gashl(v, shift), ashl(v, shift));
	    checkone(v, ">>",  shift, gashr(v, shift), ashr(v, shift));
	    checkone(v, ">>>", shift, gushr(v, shift), ushr(v, shift));

	    v = ~v;
	    checkone(v, "<<",  shift, gashl(v, shift), ashl(v, shift));
	    checkone(v, ">>",  shift, gashr(v, shift), ashr(v, shift));
	    checkone(v, ">>>", shift, gushr(v, shift), ushr(v, shift));

	    v ^= 0x80000000; // Change the state of bit31 for left shift propagation
	    checkone(v, "<<",  shift, gashl(v, shift), ashl(v, shift));
	    checkone(v, ">>",  shift, gashr(v, shift), ashr(v, shift));
	    checkone(v, ">>>", shift, gushr(v, shift), ushr(v, shift));

	    v = ~v;
	    checkone(v, "<<",  shift, gashl(v, shift), ashl(v, shift));
	    checkone(v, ">>",  shift, gashr(v, shift), ashr(v, shift));
	    checkone(v, ">>>", shift, gushr(v, shift), ushr(v, shift));

	    v ^= 0x10000000L; // Change the state of bit32 for right shift propagation
	    checkone(v, "<<",  shift, gashl(v, shift), ashl(v, shift));
	    checkone(v, ">>",  shift, gashr(v, shift), ashr(v, shift));
	    checkone(v, ">>>", shift, gushr(v, shift), ushr(v, shift));

	    v = ~v;
	    checkone(v, "<<",  shift, gashl(v, shift), ashl(v, shift));
	    checkone(v, ">>",  shift, gashr(v, shift), ashr(v, shift));
	    checkone(v, ">>>", shift, gushr(v, shift), ushr(v, shift));
	}
    }

    public static void main(String args[]) {

	// shift amounts are modulo 64 so run the checks with various shift values
	for (int shift = 0; shift < 256; shift++) {
	    checkall(shift);
	}
    }
}
