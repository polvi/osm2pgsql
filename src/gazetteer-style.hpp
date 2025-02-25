#ifndef OSM2PGSQL_GAZETTEER_STYLE_HPP
#define OSM2PGSQL_GAZETTEER_STYLE_HPP

/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm2pgsql (https://osm2pgsql.org/).
 *
 * Copyright (C) 2006-2023 by the osm2pgsql developer community.
 * For a full list of authors see the git log.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <osmium/fwd.hpp>
#include <osmium/osm/metadata_options.hpp>

#include "db-copy-mgr.hpp"

/**
 * Deleter which removes objects by osm_type, osm_id and class
 * from a gazetteer place table.
 *
 * It deletes all object that have a given type and id and where the
 * (comma-separated) list of classes does _not_ match.
 */
class db_deleter_place_t
{
    enum
    {
        // Deletion in the place table is fairly complex because of the
        // compound primary key. It is better to start earlier with the
        // deletion, so it can run in parallel with the file import.
        Max_entries = 100000
    };

    struct item_t
    {
        std::string classes;
        osmid_t osm_id;
        char osm_type;

        item_t(char t, osmid_t i, std::string c)
        : classes(std::move(c)), osm_id(i), osm_type(t)
        {}

        item_t(char t, osmid_t i) : osm_id(i), osm_type(t) {}
    };

public:
    bool has_data() const noexcept { return !m_deletables.empty(); }

    void add(char osm_type, osmid_t osm_id, std::string const &classes)
    {
        m_deletables.emplace_back(osm_type, osm_id, classes);
    }

    void add(char osm_type, osmid_t osm_id)
    {
        m_deletables.emplace_back(osm_type, osm_id);
    }

    void delete_rows(std::string const &table, std::string const &column,
                     pg_conn_t *conn);

    bool is_full() const noexcept { return m_deletables.size() > Max_entries; }

private:
    /// Vector with object to delete before copying
    std::vector<item_t> m_deletables;
};

/**
 * Copy manager for the gazetteer place table only.
 *
 * Implicitly contains the table description.
 */
class gazetteer_copy_mgr_t : public db_copy_mgr_t<db_deleter_place_t>
{
public:
    explicit gazetteer_copy_mgr_t(
        std::shared_ptr<db_copy_thread_t> const &processor)
    : db_copy_mgr_t<db_deleter_place_t>(processor),
      m_table(
          std::make_shared<db_target_descr_t>("public", "place", "place_id"))
    {
    }

    void prepare() { new_line(m_table); }

private:
    std::shared_ptr<db_target_descr_t> m_table;
};

enum : int
{
    MAX_ADMINLEVEL = 15
};

class gazetteer_style_t
{
    using flag_t = uint16_t;
    using ptag_t = std::pair<char const *, char const *>;
    using pmaintag_t = std::tuple<char const *, char const *, flag_t>;

    enum style_flags : uint16_t
    {
        SF_MAIN = 1U << 0U,
        SF_MAIN_NAMED = 1U << 1U,
        SF_MAIN_NAMED_KEY = 1U << 2U,
        SF_MAIN_FALLBACK = 1U << 3U,
        SF_MAIN_OPERATOR = 1U << 4U,
        SF_NAME = 1U << 5U,
        SF_REF = 1U << 6U,
        SF_ADDRESS = 1U << 7U,
        SF_ADDRESS_POINT = 1U << 8U,
        SF_POSTCODE = 1U << 9U,
        SF_COUNTRY = 1U << 10U,
        SF_EXTRA = 1U << 11U,
        SF_INTERPOLATION = 1U << 12U,
        SF_BOUNDARY = 1U << 13U, // internal flag for boundaries
    };

    enum class matcher_t
    {
        MT_FULL,
        MT_KEY,
        MT_PREFIX,
        MT_SUFFIX,
        MT_VALUE
    };

    struct string_with_flag_t
    {
        std::string name;
        flag_t flag;
        matcher_t type;

        string_with_flag_t(std::string n, flag_t f, matcher_t t)
        : name(std::move(n)), flag(f), type(t)
        {}
    };

    using flag_list_t = std::vector<string_with_flag_t>;

public:
    using copy_mgr_t = gazetteer_copy_mgr_t;

    void load_style(std::string const &filename);
    void process_tags(osmium::OSMObject const &o);
    void copy_out(osmium::OSMObject const &o, std::string const &geom,
                  copy_mgr_t *buffer) const;
    std::string class_list() const;

    bool has_data() const noexcept { return !m_main.empty(); }

private:
    bool add_metadata_style_entry(std::string const &key);
    void add_style_entry(std::string const &key, std::string const &value,
                         flag_t flags);
    flag_t parse_flags(std::string const &str);
    flag_t find_flag(char const *k, char const *v) const;
    void filter_main_tags(bool is_named, osmium::TagList const &tags);

    void clear();

    // Style data.
    flag_list_t m_matcher;
    flag_t m_default{0};
    bool m_any_operator_matches{false};

    // Cached OSM object data.

    /// class/type pairs to include
    std::vector<pmaintag_t> m_main;
    /// name tags to include
    std::vector<ptag_t> m_names;
    /// extratags to include
    std::vector<ptag_t> m_extra;
    /// addresstags to include
    std::vector<ptag_t> m_address;
    /// value of operator tag
    char const *m_operator = nullptr;
    /// admin level
    int m_admin_level = MAX_ADMINLEVEL;

    /// which metadata fields of the OSM objects should be written to the output
    osmium::metadata_options m_metadata_fields{"none"};
};

#endif // OSM2PGSQL_GAZETTEER_STYLE_HPP
