/*
 * Copyright © 2011,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef RB_OT_MAXP_TABLE_HH
#define RB_OT_MAXP_TABLE_HH

#include "hb-open-type.hh"

namespace OT {

/*
 * maxp -- Maximum Profile
 * https://docs.microsoft.com/en-us/typography/opentype/spec/maxp
 */

#define RB_OT_TAG_maxp RB_TAG('m', 'a', 'x', 'p')

struct maxpV1Tail
{
    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(c->check_struct(this));
    }

    HBUINT16 maxPoints;             /* Maximum points in a non-composite glyph. */
    HBUINT16 maxContours;           /* Maximum contours in a non-composite glyph. */
    HBUINT16 maxCompositePoints;    /* Maximum points in a composite glyph. */
    HBUINT16 maxCompositeContours;  /* Maximum contours in a composite glyph. */
    HBUINT16 maxZones;              /* 1 if instructions do not use the twilight zone (Z0),
                                     * or 2 if instructions do use Z0; should be set to 2 in
                                     * most cases. */
    HBUINT16 maxTwilightPoints;     /* Maximum points used in Z0. */
    HBUINT16 maxStorage;            /* Number of Storage Area locations. */
    HBUINT16 maxFunctionDefs;       /* Number of FDEFs, equal to the highest function number + 1. */
    HBUINT16 maxInstructionDefs;    /* Number of IDEFs. */
    HBUINT16 maxStackElements;      /* Maximum stack depth. (This includes Font and CVT
                                     * Programs, as well as the instructions for each glyph.) */
    HBUINT16 maxSizeOfInstructions; /* Maximum byte count for glyph instructions. */
    HBUINT16 maxComponentElements;  /* Maximum number of components referenced at
                                     * "top level" for any composite glyph. */
    HBUINT16 maxComponentDepth;     /* Maximum levels of recursion; 1 for simple components. */
public:
    DEFINE_SIZE_STATIC(26);
};

struct maxp
{
    static constexpr rb_tag_t tableTag = RB_OT_TAG_maxp;

    unsigned int get_num_glyphs() const
    {
        return numGlyphs;
    }

    void set_num_glyphs(unsigned int count)
    {
        numGlyphs = count;
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        if (unlikely(!c->check_struct(this)))
            return_trace(false);

        if (version.major == 1) {
            const maxpV1Tail &v1 = StructAfter<maxpV1Tail>(*this);
            return_trace(v1.sanitize(c));
        }
        return_trace(likely(version.major == 0 && version.minor == 0x5000u));
    }

    static void drop_hint_fields(maxpV1Tail *dest_v1)
    {
        dest_v1->maxZones = 1;
        dest_v1->maxTwilightPoints = 0;
        dest_v1->maxStorage = 0;
        dest_v1->maxFunctionDefs = 0;
        dest_v1->maxInstructionDefs = 0;
        dest_v1->maxStackElements = 0;
        dest_v1->maxSizeOfInstructions = 0;
    }

protected:
    FixedVersion<> version; /* Version of the maxp table (0.5 or 1.0),
                             * 0x00005000u or 0x00010000u. */
    HBUINT16 numGlyphs;
    /* The number of glyphs in the font. */
    /*maxpV1Tail	v1Tail[RB_VAR_ARRAY]; */
public:
    DEFINE_SIZE_STATIC(6);
};

} /* namespace OT */

#endif /* RB_OT_MAXP_TABLE_HH */
