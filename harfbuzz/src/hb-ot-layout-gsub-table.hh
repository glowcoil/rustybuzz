/*
 * Copyright © 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright © 2010,2012,2013  Google, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef RB_OT_LAYOUT_GSUB_TABLE_HH
#define RB_OT_LAYOUT_GSUB_TABLE_HH

#include "hb-ot-layout-gsubgpos.hh"

namespace OT {

typedef rb_pair_t<rb_codepoint_t, rb_codepoint_t> rb_codepoint_pair_t;

struct SingleSubstFormat1
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return (this + coverage).intersects(glyphs);
    }

    void closure(rb_closure_context_t *c) const
    {
        unsigned d = deltaGlyphID;
        +rb_iter(this + coverage) | rb_filter(*c->glyphs) |
            rb_map([d](rb_codepoint_t g) { return (g + d) & 0xFFFFu; }) | rb_sink(c->output);
    }

    void closure_lookups(rb_closure_lookups_context_t *c) const {}

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        if (unlikely(!(this + coverage).collect_coverage(c->input)))
            return;
        unsigned d = deltaGlyphID;
        +rb_iter(this + coverage) | rb_map([d](rb_codepoint_t g) { return (g + d) & 0xFFFFu; }) | rb_sink(c->output);
    }

    const Coverage &get_coverage() const
    {
        return this + coverage;
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        return c->len == 1 && (this + coverage).get_coverage(c->glyphs[0]) != NOT_COVERED;
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);

        rb_codepoint_t glyph_id = rb_buffer_get_cur(c->buffer, 0)->codepoint;
        unsigned int index = (this + coverage).get_coverage(glyph_id);
        if (likely(index == NOT_COVERED))
            return_trace(false);

        /* According to the Adobe Annotated OpenType Suite, result is always
         * limited to 16bit. */
        glyph_id = (glyph_id + deltaGlyphID) & 0xFFFFu;
        c->replace_glyph(glyph_id);

        return_trace(true);
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(coverage.sanitize(c, this) && deltaGlyphID.sanitize(c));
    }

protected:
    HBUINT16 format;             /* Format identifier--format = 1 */
    OffsetTo<Coverage> coverage; /* Offset to Coverage table--from
                                  * beginning of Substitution table */
    HBUINT16 deltaGlyphID;       /* Add to original GlyphID to get
                                  * substitute GlyphID, modulo 0x10000 */
public:
    DEFINE_SIZE_STATIC(6);
};

struct SingleSubstFormat2
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return (this + coverage).intersects(glyphs);
    }

    void closure(rb_closure_context_t *c) const
    {
        +rb_zip(this + coverage, substitute) | rb_filter(*c->glyphs, rb_first) | rb_map(rb_second) | rb_sink(c->output);
    }

    void closure_lookups(rb_closure_lookups_context_t *c) const {}

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        if (unlikely(!(this + coverage).collect_coverage(c->input)))
            return;
        +rb_zip(this + coverage, substitute) | rb_map(rb_second) | rb_sink(c->output);
    }

    const Coverage &get_coverage() const
    {
        return this + coverage;
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        return c->len == 1 && (this + coverage).get_coverage(c->glyphs[0]) != NOT_COVERED;
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);
        unsigned int index = (this + coverage).get_coverage(rb_buffer_get_cur(c->buffer, 0)->codepoint);
        if (likely(index == NOT_COVERED))
            return_trace(false);

        if (unlikely(index >= substitute.len))
            return_trace(false);

        c->replace_glyph(substitute[index]);

        return_trace(true);
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(coverage.sanitize(c, this) && substitute.sanitize(c));
    }

protected:
    HBUINT16 format;               /* Format identifier--format = 2 */
    OffsetTo<Coverage> coverage;   /* Offset to Coverage table--from
                                    * beginning of Substitution table */
    ArrayOf<HBGlyphID> substitute; /* Array of substitute
                                    * GlyphIDs--ordered by Coverage Index */
public:
    DEFINE_SIZE_ARRAY(6, substitute);
};

struct SingleSubst
{
    template <typename context_t, typename... Ts> typename context_t::return_t dispatch(context_t *c, Ts &&... ds) const
    {
        TRACE_DISPATCH(this, u.format);
        if (unlikely(!c->may_dispatch(this, &u.format)))
            return_trace(c->no_dispatch_return_value());
        switch (u.format) {
        case 1:
            return_trace(c->dispatch(u.format1, rb_forward<Ts>(ds)...));
        case 2:
            return_trace(c->dispatch(u.format2, rb_forward<Ts>(ds)...));
        default:
            return_trace(c->default_return_value());
        }
    }

protected:
    union {
        HBUINT16 format; /* Format identifier */
        SingleSubstFormat1 format1;
        SingleSubstFormat2 format2;
    } u;
};

struct Sequence
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return rb_all(substitute, glyphs);
    }

    void closure(rb_closure_context_t *c) const
    {
        c->output->add_array(substitute.arrayZ, substitute.len);
    }

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        c->output->add_array(substitute.arrayZ, substitute.len);
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);
        unsigned int count = substitute.len;

        /* Special-case to make it in-place and not consider this
         * as a "multiplied" substitution. */
        if (unlikely(count == 1)) {
            c->replace_glyph(substitute.arrayZ[0]);
            return_trace(true);
        }
        /* Spec disallows this, but Uniscribe allows it.
         * https://github.com/harfbuzz/harfbuzz/issues/253 */
        else if (unlikely(count == 0)) {
            rb_buffer_delete_glyph(c->buffer);
            return_trace(true);
        }

        unsigned int klass =
            _rb_glyph_info_is_ligature(rb_buffer_get_cur(c->buffer, 0)) ? RB_OT_LAYOUT_GLYPH_PROPS_BASE_GLYPH : 0;

        for (unsigned int i = 0; i < count; i++) {
            _rb_glyph_info_set_lig_props_for_component(rb_buffer_get_cur(c->buffer, 0), i);
            c->output_glyph_for_component(substitute.arrayZ[i], klass);
        }
        rb_buffer_skip_glyph(c->buffer);

        return_trace(true);
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(substitute.sanitize(c));
    }

protected:
    ArrayOf<HBGlyphID> substitute; /* String of GlyphIDs to substitute */
public:
    DEFINE_SIZE_ARRAY(2, substitute);
};

struct MultipleSubstFormat1
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return (this + coverage).intersects(glyphs);
    }

    void closure(rb_closure_context_t *c) const
    {
        +rb_zip(this + coverage, sequence) | rb_filter(*c->glyphs, rb_first) | rb_map(rb_second) |
            rb_map(rb_add(this)) | rb_apply([c](const Sequence &_) { _.closure(c); });
    }

    void closure_lookups(rb_closure_lookups_context_t *c) const {}

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        if (unlikely(!(this + coverage).collect_coverage(c->input)))
            return;
        +rb_zip(this + coverage, sequence) | rb_map(rb_second) | rb_map(rb_add(this)) |
            rb_apply([c](const Sequence &_) { _.collect_glyphs(c); });
    }

    const Coverage &get_coverage() const
    {
        return this + coverage;
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        return c->len == 1 && (this + coverage).get_coverage(c->glyphs[0]) != NOT_COVERED;
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);

        unsigned int index = (this + coverage).get_coverage(rb_buffer_get_cur(c->buffer, 0)->codepoint);
        if (likely(index == NOT_COVERED))
            return_trace(false);

        return_trace((this + sequence[index]).apply(c));
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(coverage.sanitize(c, this) && sequence.sanitize(c, this));
    }

protected:
    HBUINT16 format;                  /* Format identifier--format = 1 */
    OffsetTo<Coverage> coverage;      /* Offset to Coverage table--from
                                       * beginning of Substitution table */
    OffsetArrayOf<Sequence> sequence; /* Array of Sequence tables
                                       * ordered by Coverage Index */
public:
    DEFINE_SIZE_ARRAY(6, sequence);
};

struct MultipleSubst
{
    template <typename context_t, typename... Ts> typename context_t::return_t dispatch(context_t *c, Ts &&... ds) const
    {
        TRACE_DISPATCH(this, u.format);
        if (unlikely(!c->may_dispatch(this, &u.format)))
            return_trace(c->no_dispatch_return_value());
        switch (u.format) {
        case 1:
            return_trace(c->dispatch(u.format1, rb_forward<Ts>(ds)...));
        default:
            return_trace(c->default_return_value());
        }
    }

protected:
    union {
        HBUINT16 format; /* Format identifier */
        MultipleSubstFormat1 format1;
    } u;
};

struct AlternateSet
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return rb_any(alternates, glyphs);
    }

    void closure(rb_closure_context_t *c) const
    {
        c->output->add_array(alternates.arrayZ, alternates.len);
    }

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        c->output->add_array(alternates.arrayZ, alternates.len);
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);
        unsigned int count = alternates.len;

        if (unlikely(!count))
            return_trace(false);

        rb_mask_t glyph_mask = rb_buffer_get_cur(c->buffer, 0)->mask;
        rb_mask_t lookup_mask = c->lookup_mask;

        /* Note: This breaks badly if two features enabled this lookup together. */
        unsigned int shift = rb_ctz(lookup_mask);
        unsigned int alt_index = ((lookup_mask & glyph_mask) >> shift);

        /* If alt_index is MAX_VALUE, randomize feature if it is the rand feature. */
        if (alt_index == RB_OT_MAP_MAX_VALUE && c->random)
            alt_index = c->random_number() % count + 1;

        if (unlikely(alt_index > count || alt_index == 0))
            return_trace(false);

        c->replace_glyph(alternates[alt_index - 1]);

        return_trace(true);
    }

    unsigned get_alternates(unsigned start_offset,
                            unsigned *alternate_count /* IN/OUT.  May be NULL. */,
                            rb_codepoint_t *alternate_glyphs /* OUT.     May be NULL. */) const
    {
        if (alternates.len && alternate_count) {
            +alternates.sub_array(start_offset, alternate_count) |
                rb_sink(rb_array(alternate_glyphs, *alternate_count));
        }
        return alternates.len;
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(alternates.sanitize(c));
    }

protected:
    ArrayOf<HBGlyphID> alternates; /* Array of alternate GlyphIDs--in
                                    * arbitrary order */
public:
    DEFINE_SIZE_ARRAY(2, alternates);
};

struct AlternateSubstFormat1
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return (this + coverage).intersects(glyphs);
    }

    void closure(rb_closure_context_t *c) const
    {
        +rb_zip(this + coverage, alternateSet) | rb_filter(c->glyphs, rb_first) | rb_map(rb_second) |
            rb_map(rb_add(this)) | rb_apply([c](const AlternateSet &_) { _.closure(c); });
    }

    void closure_lookups(rb_closure_lookups_context_t *c) const {}

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        if (unlikely(!(this + coverage).collect_coverage(c->input)))
            return;
        +rb_zip(this + coverage, alternateSet) | rb_map(rb_second) | rb_map(rb_add(this)) |
            rb_apply([c](const AlternateSet &_) { _.collect_glyphs(c); });
    }

    const Coverage &get_coverage() const
    {
        return this + coverage;
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        return c->len == 1 && (this + coverage).get_coverage(c->glyphs[0]) != NOT_COVERED;
    }

    unsigned get_glyph_alternates(rb_codepoint_t gid,
                                  unsigned start_offset,
                                  unsigned *alternate_count /* IN/OUT.  May be NULL. */,
                                  rb_codepoint_t *alternate_glyphs /* OUT.     May be NULL. */) const
    {
        return (this + alternateSet[(this + coverage).get_coverage(gid)])
            .get_alternates(start_offset, alternate_count, alternate_glyphs);
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);

        unsigned int index = (this + coverage).get_coverage(rb_buffer_get_cur(c->buffer, 0)->codepoint);
        if (likely(index == NOT_COVERED))
            return_trace(false);

        return_trace((this + alternateSet[index]).apply(c));
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(coverage.sanitize(c, this) && alternateSet.sanitize(c, this));
    }

protected:
    HBUINT16 format;                          /* Format identifier--format = 1 */
    OffsetTo<Coverage> coverage;              /* Offset to Coverage table--from
                                               * beginning of Substitution table */
    OffsetArrayOf<AlternateSet> alternateSet; /* Array of AlternateSet tables
                                               * ordered by Coverage Index */
public:
    DEFINE_SIZE_ARRAY(6, alternateSet);
};

struct AlternateSubst
{
    template <typename context_t, typename... Ts> typename context_t::return_t dispatch(context_t *c, Ts &&... ds) const
    {
        TRACE_DISPATCH(this, u.format);
        if (unlikely(!c->may_dispatch(this, &u.format)))
            return_trace(c->no_dispatch_return_value());
        switch (u.format) {
        case 1:
            return_trace(c->dispatch(u.format1, rb_forward<Ts>(ds)...));
        default:
            return_trace(c->default_return_value());
        }
    }

protected:
    union {
        HBUINT16 format; /* Format identifier */
        AlternateSubstFormat1 format1;
    } u;
};

struct Ligature
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return rb_all(component, glyphs);
    }

    void closure(rb_closure_context_t *c) const
    {
        if (!intersects(c->glyphs))
            return;
        c->output->add(ligGlyph);
    }

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        c->input->add_array(component.arrayZ, component.get_length());
        c->output->add(ligGlyph);
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        if (c->len != component.lenP1)
            return false;

        for (unsigned int i = 1; i < c->len; i++)
            if (likely(c->glyphs[i] != component[i]))
                return false;

        return true;
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);
        unsigned int count = component.lenP1;

        if (unlikely(!count))
            return_trace(false);

        /* Special-case to make it in-place and not consider this
         * as a "ligated" substitution. */
        if (unlikely(count == 1)) {
            c->replace_glyph(ligGlyph);
            return_trace(true);
        }

        unsigned int total_component_count = 0;

        unsigned int match_length = 0;
        unsigned int match_positions[RB_MAX_CONTEXT_LENGTH];

        if (likely(!match_input(
                c, count, &component[1], match_glyph, nullptr, &match_length, match_positions, &total_component_count)))
            return_trace(false);

        ligate_input(c, count, match_positions, match_length, ligGlyph, total_component_count);

        return_trace(true);
    }

public:
    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(ligGlyph.sanitize(c) && component.sanitize(c));
    }

protected:
    HBGlyphID ligGlyph;                   /* GlyphID of ligature to substitute */
    HeadlessArrayOf<HBGlyphID> component; /* Array of component GlyphIDs--start
                                           * with the second  component--ordered
                                           * in writing direction */
public:
    DEFINE_SIZE_ARRAY(4, component);
};

struct LigatureSet
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return +rb_iter(ligature) | rb_map(rb_add(this)) |
               rb_map([glyphs](const Ligature &_) { return _.intersects(glyphs); }) | rb_any;
    }

    void closure(rb_closure_context_t *c) const
    {
        +rb_iter(ligature) | rb_map(rb_add(this)) | rb_apply([c](const Ligature &_) { _.closure(c); });
    }

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        +rb_iter(ligature) | rb_map(rb_add(this)) | rb_apply([c](const Ligature &_) { _.collect_glyphs(c); });
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        return +rb_iter(ligature) | rb_map(rb_add(this)) | rb_map([c](const Ligature &_) { return _.would_apply(c); }) |
               rb_any;
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);
        unsigned int num_ligs = ligature.len;
        for (unsigned int i = 0; i < num_ligs; i++) {
            const Ligature &lig = this + ligature[i];
            if (lig.apply(c))
                return_trace(true);
        }

        return_trace(false);
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(ligature.sanitize(c, this));
    }

protected:
    OffsetArrayOf<Ligature> ligature; /* Array LigatureSet tables
                                       * ordered by preference */
public:
    DEFINE_SIZE_ARRAY(2, ligature);
};

struct LigatureSubstFormat1
{
    bool intersects(const rb_set_t *glyphs) const
    {
        return +rb_zip(this + coverage, ligatureSet) | rb_filter(*glyphs, rb_first) | rb_map(rb_second) |
               rb_map([this, glyphs](const OffsetTo<LigatureSet> &_) { return (this + _).intersects(glyphs); }) |
               rb_any;
    }

    void closure(rb_closure_context_t *c) const
    {
        +rb_zip(this + coverage, ligatureSet) | rb_filter(*c->glyphs, rb_first) | rb_map(rb_second) |
            rb_map(rb_add(this)) | rb_apply([c](const LigatureSet &_) { _.closure(c); });
    }

    void closure_lookups(rb_closure_lookups_context_t *c) const {}

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        if (unlikely(!(this + coverage).collect_coverage(c->input)))
            return;

        +rb_zip(this + coverage, ligatureSet) | rb_map(rb_second) | rb_map(rb_add(this)) |
            rb_apply([c](const LigatureSet &_) { _.collect_glyphs(c); });
    }

    const Coverage &get_coverage() const
    {
        return this + coverage;
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        unsigned int index = (this + coverage).get_coverage(c->glyphs[0]);
        if (likely(index == NOT_COVERED))
            return false;

        const LigatureSet &lig_set = this + ligatureSet[index];
        return lig_set.would_apply(c);
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);

        unsigned int index = (this + coverage).get_coverage(rb_buffer_get_cur(c->buffer, 0)->codepoint);
        if (likely(index == NOT_COVERED))
            return_trace(false);

        const LigatureSet &lig_set = this + ligatureSet[index];
        return_trace(lig_set.apply(c));
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        return_trace(coverage.sanitize(c, this) && ligatureSet.sanitize(c, this));
    }

protected:
    HBUINT16 format;                        /* Format identifier--format = 1 */
    OffsetTo<Coverage> coverage;            /* Offset to Coverage table--from
                                             * beginning of Substitution table */
    OffsetArrayOf<LigatureSet> ligatureSet; /* Array LigatureSet tables
                                             * ordered by Coverage Index */
public:
    DEFINE_SIZE_ARRAY(6, ligatureSet);
};

struct LigatureSubst
{
    template <typename context_t, typename... Ts> typename context_t::return_t dispatch(context_t *c, Ts &&... ds) const
    {
        TRACE_DISPATCH(this, u.format);
        if (unlikely(!c->may_dispatch(this, &u.format)))
            return_trace(c->no_dispatch_return_value());
        switch (u.format) {
        case 1:
            return_trace(c->dispatch(u.format1, rb_forward<Ts>(ds)...));
        default:
            return_trace(c->default_return_value());
        }
    }

protected:
    union {
        HBUINT16 format; /* Format identifier */
        LigatureSubstFormat1 format1;
    } u;
};

struct ContextSubst : Context
{
};

struct ChainContextSubst : ChainContext
{
};

struct ExtensionSubst : Extension<ExtensionSubst>
{
    typedef struct SubstLookupSubTable SubTable;
    bool is_reverse() const;
};

struct ReverseChainSingleSubstFormat1
{
    bool intersects(const rb_set_t *glyphs) const
    {
        if (!(this + coverage).intersects(glyphs))
            return false;

        const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage>>(backtrack);

        unsigned int count;

        count = backtrack.len;
        for (unsigned int i = 0; i < count; i++)
            if (!(this + backtrack[i]).intersects(glyphs))
                return false;

        count = lookahead.len;
        for (unsigned int i = 0; i < count; i++)
            if (!(this + lookahead[i]).intersects(glyphs))
                return false;

        return true;
    }

    void closure(rb_closure_context_t *c) const
    {
        if (!intersects(c->glyphs))
            return;

        const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage>>(backtrack);
        const ArrayOf<HBGlyphID> &substitute = StructAfter<ArrayOf<HBGlyphID>>(lookahead);

        +rb_zip(this + coverage, substitute) | rb_filter(*c->glyphs, rb_first) | rb_map(rb_second) | rb_sink(c->output);
    }

    void closure_lookups(rb_closure_lookups_context_t *c) const {}

    void collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        if (unlikely(!(this + coverage).collect_coverage(c->input)))
            return;

        unsigned int count;

        count = backtrack.len;
        for (unsigned int i = 0; i < count; i++)
            if (unlikely(!(this + backtrack[i]).collect_coverage(c->before)))
                return;

        const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage>>(backtrack);
        count = lookahead.len;
        for (unsigned int i = 0; i < count; i++)
            if (unlikely(!(this + lookahead[i]).collect_coverage(c->after)))
                return;

        const ArrayOf<HBGlyphID> &substitute = StructAfter<ArrayOf<HBGlyphID>>(lookahead);
        count = substitute.len;
        c->output->add_array(substitute.arrayZ, substitute.len);
    }

    const Coverage &get_coverage() const
    {
        return this + coverage;
    }

    bool would_apply(rb_would_apply_context_t *c) const
    {
        return c->len == 1 && (this + coverage).get_coverage(c->glyphs[0]) != NOT_COVERED;
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);
        if (unlikely(c->nesting_level_left != RB_MAX_NESTING_LEVEL))
            return_trace(false); /* No chaining to this type */

        unsigned int index = (this + coverage).get_coverage(rb_buffer_get_cur(c->buffer, 0)->codepoint);
        if (likely(index == NOT_COVERED))
            return_trace(false);

        const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage>>(backtrack);
        const ArrayOf<HBGlyphID> &substitute = StructAfter<ArrayOf<HBGlyphID>>(lookahead);

        if (unlikely(index >= substitute.len))
            return_trace(false);

        unsigned int start_index = 0, end_index = 0;
        if (match_backtrack(c, backtrack.len, (HBUINT16 *)backtrack.arrayZ, match_coverage, this, &start_index) &&
            match_lookahead(c, lookahead.len, (HBUINT16 *)lookahead.arrayZ, match_coverage, this, 1, &end_index)) {
            rb_buffer_unsafe_to_break_from_outbuffer(c->buffer, start_index, end_index);
            c->replace_glyph_inplace(substitute[index]);
            /* Note: We DON'T decrease buffer->idx.  The main loop does it
             * for us.  This is useful for preventing surprises if someone
             * calls us through a Context lookup. */
            return_trace(true);
        }

        return_trace(false);
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        TRACE_SANITIZE(this);
        if (!(coverage.sanitize(c, this) && backtrack.sanitize(c, this)))
            return_trace(false);
        const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage>>(backtrack);
        if (!lookahead.sanitize(c, this))
            return_trace(false);
        const ArrayOf<HBGlyphID> &substitute = StructAfter<ArrayOf<HBGlyphID>>(lookahead);
        return_trace(substitute.sanitize(c));
    }

protected:
    HBUINT16 format;                    /* Format identifier--format = 1 */
    OffsetTo<Coverage> coverage;        /* Offset to Coverage table--from
                                         * beginning of table */
    OffsetArrayOf<Coverage> backtrack;  /* Array of coverage tables
                                         * in backtracking sequence, in glyph
                                         * sequence order */
    OffsetArrayOf<Coverage> lookaheadX; /* Array of coverage tables
                                         * in lookahead sequence, in glyph
                                         * sequence order */
    ArrayOf<HBGlyphID> substituteX;     /* Array of substitute
                                         * GlyphIDs--ordered by Coverage Index */
public:
    DEFINE_SIZE_MIN(10);
};

struct ReverseChainSingleSubst
{
    template <typename context_t, typename... Ts> typename context_t::return_t dispatch(context_t *c, Ts &&... ds) const
    {
        TRACE_DISPATCH(this, u.format);
        if (unlikely(!c->may_dispatch(this, &u.format)))
            return_trace(c->no_dispatch_return_value());
        switch (u.format) {
        case 1:
            return_trace(c->dispatch(u.format1, rb_forward<Ts>(ds)...));
        default:
            return_trace(c->default_return_value());
        }
    }

protected:
    union {
        HBUINT16 format; /* Format identifier */
        ReverseChainSingleSubstFormat1 format1;
    } u;
};

/*
 * SubstLookup
 */

struct SubstLookupSubTable
{
    friend struct Lookup;
    friend struct SubstLookup;

    enum Type {
        Single = 1,
        Multiple = 2,
        Alternate = 3,
        Ligature = 4,
        Context = 5,
        ChainContext = 6,
        Extension = 7,
        ReverseChainSingle = 8
    };

    template <typename context_t, typename... Ts>
    typename context_t::return_t dispatch(context_t *c, unsigned int lookup_type, Ts &&... ds) const
    {
        TRACE_DISPATCH(this, lookup_type);
        switch (lookup_type) {
        case Single:
            return_trace(u.single.dispatch(c, rb_forward<Ts>(ds)...));
        case Multiple:
            return_trace(u.multiple.dispatch(c, rb_forward<Ts>(ds)...));
        case Alternate:
            return_trace(u.alternate.dispatch(c, rb_forward<Ts>(ds)...));
        case Ligature:
            return_trace(u.ligature.dispatch(c, rb_forward<Ts>(ds)...));
        case Context:
            return_trace(u.context.dispatch(c, rb_forward<Ts>(ds)...));
        case ChainContext:
            return_trace(u.chainContext.dispatch(c, rb_forward<Ts>(ds)...));
        case Extension:
            return_trace(u.extension.dispatch(c, rb_forward<Ts>(ds)...));
        case ReverseChainSingle:
            return_trace(u.reverseChainContextSingle.dispatch(c, rb_forward<Ts>(ds)...));
        default:
            return_trace(c->default_return_value());
        }
    }

    bool intersects(const rb_set_t *glyphs, unsigned int lookup_type) const
    {
        rb_intersects_context_t c(glyphs);
        return dispatch(&c, lookup_type);
    }

protected:
    union {
        SingleSubst single;
        MultipleSubst multiple;
        AlternateSubst alternate;
        LigatureSubst ligature;
        ContextSubst context;
        ChainContextSubst chainContext;
        ExtensionSubst extension;
        ReverseChainSingleSubst reverseChainContextSingle;
    } u;

public:
    DEFINE_SIZE_MIN(0);
};

struct SubstLookup : Lookup
{
    typedef SubstLookupSubTable SubTable;

    const SubTable &get_subtable(unsigned int i) const
    {
        return Lookup::get_subtable<SubTable>(i);
    }

    static inline bool lookup_type_is_reverse(unsigned int lookup_type)
    {
        return lookup_type == SubTable::ReverseChainSingle;
    }

    bool is_reverse() const
    {
        unsigned int type = get_type();
        if (unlikely(type == SubTable::Extension))
            return reinterpret_cast<const ExtensionSubst &>(get_subtable(0)).is_reverse();
        return lookup_type_is_reverse(type);
    }

    bool apply(rb_ot_apply_context_t *c) const
    {
        TRACE_APPLY(this);
        return_trace(dispatch(c));
    }

    bool intersects(const rb_set_t *glyphs) const
    {
        rb_intersects_context_t c(glyphs);
        return dispatch(&c);
    }

    rb_closure_context_t::return_t closure(rb_closure_context_t *c, unsigned int this_index) const
    {
        if (!c->should_visit_lookup(this_index))
            return rb_closure_context_t::default_return_value();

        c->set_recurse_func(dispatch_closure_recurse_func);

        rb_closure_context_t::return_t ret = dispatch(c);

        c->flush();

        return ret;
    }

    rb_closure_lookups_context_t::return_t closure_lookups(rb_closure_lookups_context_t *c, unsigned this_index) const
    {
        if (c->is_lookup_visited(this_index))
            return rb_closure_lookups_context_t::default_return_value();

        c->set_lookup_visited(this_index);
        if (!intersects(c->glyphs)) {
            c->set_lookup_inactive(this_index);
            return rb_closure_lookups_context_t::default_return_value();
        }

        c->set_recurse_func(dispatch_closure_lookups_recurse_func);

        rb_closure_lookups_context_t::return_t ret = dispatch(c);
        return ret;
    }

    rb_collect_glyphs_context_t::return_t collect_glyphs(rb_collect_glyphs_context_t *c) const
    {
        c->set_recurse_func(dispatch_recurse_func<rb_collect_glyphs_context_t>);
        return dispatch(c);
    }

    template <typename set_t> void collect_coverage(set_t *glyphs) const
    {
        rb_collect_coverage_context_t<set_t> c(glyphs);
        dispatch(&c);
    }

    bool would_apply(rb_would_apply_context_t *c, const rb_ot_layout_lookup_accelerator_t *accel) const
    {
        if (unlikely(!c->len))
            return false;
        if (!accel->may_have(c->glyphs[0]))
            return false;
        return dispatch(c);
    }

    static inline bool apply_recurse_func(rb_ot_apply_context_t *c, unsigned int lookup_index);

    template <typename context_t>
    static inline typename context_t::return_t dispatch_recurse_func(context_t *c, unsigned int lookup_index);

    static inline rb_closure_context_t::return_t dispatch_closure_recurse_func(rb_closure_context_t *c,
                                                                               unsigned int lookup_index)
    {
        if (!c->should_visit_lookup(lookup_index))
            return rb_empty_t();

        rb_closure_context_t::return_t ret = dispatch_recurse_func(c, lookup_index);

        /* While in theory we should flush here, it will cause timeouts because a recursive
         * lookup can keep growing the glyph set.  Skip, and outer loop will retry up to
         * RB_CLOSURE_MAX_STAGES time, which should be enough for every realistic font. */
        // c->flush ();

        return ret;
    }

    RB_INTERNAL static rb_closure_lookups_context_t::return_t
    dispatch_closure_lookups_recurse_func(rb_closure_lookups_context_t *c, unsigned lookup_index);

    template <typename context_t, typename... Ts> typename context_t::return_t dispatch(context_t *c, Ts &&... ds) const
    {
        return Lookup::dispatch<SubTable>(c, rb_forward<Ts>(ds)...);
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        return Lookup::sanitize<SubTable>(c);
    }
};

/*
 * GSUB -- Glyph Substitution
 * https://docs.microsoft.com/en-us/typography/opentype/spec/gsub
 */

struct GSUB : GSUBGPOS
{
    static constexpr rb_tag_t tableTag = RB_OT_TAG_GSUB;

    const SubstLookup &get_lookup(unsigned int i) const
    {
        return static_cast<const SubstLookup &>(GSUBGPOS::get_lookup(i));
    }

    bool sanitize(rb_sanitize_context_t *c) const
    {
        return GSUBGPOS::sanitize<SubstLookup>(c);
    }

    RB_INTERNAL bool is_blocklisted(rb_blob_t *blob, rb_face_t *face) const;

    void closure_lookups(rb_face_t *face, const rb_set_t *glyphs, rb_set_t *lookup_indexes /* IN/OUT */) const
    {
        GSUBGPOS::closure_lookups<SubstLookup>(face, glyphs, lookup_indexes);
    }

    typedef GSUBGPOS::accelerator_t<GSUB> accelerator_t;
};

struct GSUB_accelerator_t : GSUB::accelerator_t
{
};

/* Out-of-class implementation for methods recursing */

/*static*/ inline bool ExtensionSubst::is_reverse() const
{
    return SubstLookup::lookup_type_is_reverse(get_type());
}
template <typename context_t>
/*static*/ typename context_t::return_t SubstLookup::dispatch_recurse_func(context_t *c, unsigned int lookup_index)
{
    const SubstLookup &l = c->face->table.GSUB.get_relaxed()->table->get_lookup(lookup_index);
    return l.dispatch(c);
}

/*static*/ inline rb_closure_lookups_context_t::return_t
SubstLookup::dispatch_closure_lookups_recurse_func(rb_closure_lookups_context_t *c, unsigned this_index)
{
    const SubstLookup &l = c->face->table.GSUB.get_relaxed()->table->get_lookup(this_index);
    return l.closure_lookups(c, this_index);
}

/*static*/ bool SubstLookup::apply_recurse_func(rb_ot_apply_context_t *c, unsigned int lookup_index)
{
    const SubstLookup &l = c->face->table.GSUB.get_relaxed()->table->get_lookup(lookup_index);
    unsigned int saved_lookup_props = c->lookup_props;
    unsigned int saved_lookup_index = c->lookup_index;
    c->set_lookup_index(lookup_index);
    c->set_lookup_props(l.get_props());
    bool ret = l.dispatch(c);
    c->set_lookup_index(saved_lookup_index);
    c->set_lookup_props(saved_lookup_props);
    return ret;
}

} /* namespace OT */

#endif /* RB_OT_LAYOUT_GSUB_TABLE_HH */
