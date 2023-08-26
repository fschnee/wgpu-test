#pragma once

#include "utils/aliases.hpp"
#include "utils/forward.hpp"
#include "object.hpp"
#include "mesh.hpp"

#include <vector>
#include <tuple>

#include <fmt/core.h>

// Utils for managing this type-mess.
namespace ghuva::impl
{
    template <typename... Ts> struct type_list;

    // Merge.
    template < typename TList1, typename TList2 > struct tlist_merge;
    template <
        template <typename...> typename TList, typename... Ts,
        template <typename...> typename TList2, typename... Ts2
    >
    struct tlist_merge< TList<Ts...>, TList2<Ts2...> > { using type = type_list<Ts..., Ts2...>; };

    // Extract.
    template < typename TList1, template <typename...> typename TList2 > struct tlist_extract;
    template <
        template <typename...> typename TList, typename... Ts,
        template <typename...> typename TList2
    >
    struct tlist_extract< TList<Ts...>, TList2 > { using type = TList2<Ts...>; };

    // Contains.
    template <typename TList, typename T>
    struct tlist_contains;
    template <
        template <typename...> typename TList,
        typename... Ts,
        typename T
    >
    struct tlist_contains< TList<T, Ts...>, T> { static constexpr bool value = true; };
    template <
        template <typename...> typename TList,
        typename... Ts,
        typename T,
        typename T2
    >
    struct tlist_contains< TList<T2, Ts...>, T> : tlist_contains< TList<Ts...>, T> {};
    template <
        template <typename...> typename TList,
        typename T
    >
    struct tlist_contains< TList<>, T> { static constexpr bool value = false; };

    // Cond.
    template <bool B, typename T, typename T2> struct cond;
    template <        typename T, typename T2> struct cond<true, T, T2>  { using type = T; };
    template <        typename T, typename T2> struct cond<false, T, T2> { using type = T2; };
    template <bool B, typename T, typename T2> using  cond_t = cond<B, T, T2>::type;
}

// Events, Messages and Boards.
namespace ghuva
{
    template <typename T>
    struct event
    {
        using body_t = T;
        u64 id;
        u64 source_object_id; // 0 = engine/external/unidentified events.
        u64 posted_at_tick;
        body_t body;
    };
    template <typename T> struct is_event             { static constexpr bool value = false; };
    template <typename T> struct is_event< event<T> > { static constexpr bool value = true; };
    template <typename T> constexpr auto is_event_v = is_event<T>::value;

    // TODO: how do I make ordering for this deterministic ?
    template <typename T>
    struct message
    {
        using body_t = T;
        u64 id;
        u64 target_object_id;
        u64 source_object_id; // 0 = engine/external/unidentified events.
        u64 posted_at_tick;
        body_t body;
    };
    template <typename T> struct is_message               { static constexpr bool value = false; };
    template <typename T> struct is_message< message<T> > { static constexpr bool value = true; };
    template <typename T> constexpr auto is_message_v = is_message<T>::value;

    template <typename... Regions>
    struct board_t
    {
        using supported_regions = impl::type_list<Regions...>;

        template <typename R>
        static constexpr auto supports = impl::tlist_contains<
            supported_regions,
            ghuva::remove_cvref_t<R>
        >::value;

        template <typename T>
        constexpr auto get() -> std::vector<T>&
        { return std::get< std::vector<T> >(regions); }

        template <typename T>
        constexpr auto get() const -> std::vector<T> const&
        { return std::get< std::vector<T> >(regions); }

        std::tuple< std::vector<Regions>... > regions;
    };
}

// Engine proper.
namespace ghuva
{
    struct engine_config
    {
        f32 leftover_tick_seconds = 0.0f;
        f32 ticks_per_second = 120.0f;

        f32 total_dt = 0.0f;

        u64 last_mesh_id = 1;
        u64 last_object_id = 1;
        u64 last_event_id = 1;
        u64 last_message_id = 1;
    };

    template <typename ExtraPostboardEvents, typename Messages>
    struct engine
    {
        using object_t = ghuva::object<engine>; // CRTP this bitch.

        // Some engine events.
        struct register_mesh   { ghuva::mesh mesh; /* Id is overriden */ };
        struct register_object { object_t object; /* Id is overriden. */ };
        struct delete_object   { u64 id; bool success; };
        struct change_tps      { f32 tps; };
        struct set_camera      { u64 object_id; };
        using  e_register_mesh   = ghuva::event< register_mesh >;
        using  e_register_object = ghuva::event< register_object >;
        using  e_delete_object   = ghuva::event< delete_object >;
        using  e_change_tps      = ghuva::event< change_tps >;
        using  e_set_camera      = ghuva::event< set_camera >;

        using default_events = impl::type_list<
            e_register_mesh,
            e_register_object,
            e_delete_object,
            e_change_tps,
            e_set_camera
        >;
        using extra_events   = ExtraPostboardEvents;
        using events         = impl::tlist_merge< default_events, extra_events >::type;
        using postboard_t    = impl::tlist_extract< events, ghuva::board_t >::type;
        using messages       = Messages;
        using messageboard_t = impl::tlist_extract< messages, ghuva::board_t >::type;

        struct snapshot
        {
            u64 id;
            u64 camera_object_id;

            std::vector<object_t> objects;

            // Shorthand for checking all the vents of a given type on the postboard. Use like:
            //     snapshot.template on_post<my_event_type>([&, count = 0](my_event_type const& e){
            //         ++count;
            //         fmt::print("Found event");
            //     })
            // You can also chain them for less typing.
            template <typename E, typename F> constexpr auto on_post(F&& f)       -> snapshot&;
            template <typename E, typename F> constexpr auto on_post(F&& f) const -> snapshot const&;

            // Calls f with each std::vector<event> in the postboard.
            template <typename F> constexpr auto all_posts(F&& f) const -> snapshot const&;

            // Since objects only see other object as const& they can only call this on theirselves.
            // constexpr auto read_messages(object&, F&&);

            // General events acessible to all objects are posted
            // to the postboard, it's up to each object to check
            // it for events that it finds relevant.
            // By nature of it's mechanism it is always 1 tick behind.
            postboard_t postboard;

            ghuva::engine_config engine_config;

        private:
            // Messages are private, no looksies.
            messageboard_t messageboard;

            // Except for you, you're ok.
            // Needs access to reorder the messageboard by target_object_id.
            // This way, the messages for each object and message type are contiguous
            // in memory and we can build a view from them.
            friend class engine;
        };

        // TODO: make all of these thread-safe.
        constexpr auto take_snapshot() -> snapshot { return last_snapshot; }
        constexpr auto tick(f32 real_dt) -> u64;

        // Posts events to the next snapshot's postboard. Returns the event id.
        // Remember that an id = 0 means failed.
        template <typename E>
        constexpr auto post(E&& event, u64 source_id = 0) -> u64;
        // TODO: Sends a direct event to the object for next snapshot.
        template <typename E>
        constexpr auto message(E&& event, u64 target_id, u64 source_id = 0) -> u64;

        // TODO: what to do about you ?
        std::vector<mesh> meshes = {};

    private:
        // TODO: snapshot keepalive system ?
        snapshot last_snapshot;
        snapshot partial_snapshot; // Postboard of this one and
                                   // other stuff are filled in
                                   // as we tick the current snapshot.

        constexpr auto fixed_tick(f32 dt) -> void;
    };

    using default_engine = engine< impl::type_list<>, impl::type_list<> >;
}

// Impls.

// TODO: config na verdade deveria apontar para partial_snapshot.config.
template <typename T, typename T2>
constexpr auto ghuva::engine<T, T2>::tick(f32 real_dt) -> u64
{
    this->partial_snapshot.engine_config.leftover_tick_seconds += real_dt;

    const auto start_tick = this->last_snapshot.id;

    while(this->partial_snapshot.engine_config.leftover_tick_seconds >= 1.0f / this->last_snapshot.engine_config.ticks_per_second)
    {
        const auto seconds_per_tick = 1.0f / this->last_snapshot.engine_config.ticks_per_second;
        this->fixed_tick(seconds_per_tick);
        this->partial_snapshot.engine_config.leftover_tick_seconds -= seconds_per_tick;
    }

    return this->last_snapshot.id - start_tick;
}

template <typename T, typename T2>
constexpr auto ghuva::engine<T, T2>::fixed_tick(f32 dt) -> void
{
    this->partial_snapshot.engine_config.total_dt += dt;
    this->partial_snapshot.id               = this->last_snapshot.id + 1;
    this->partial_snapshot.objects          = this->last_snapshot.objects;
    this->partial_snapshot.camera_object_id = this->last_snapshot.camera_object_id;

    // Run the engine event handlers.
    this->partial_snapshot.template on_post<e_delete_object>([&](auto& e){
        auto& objs = this->partial_snapshot.objects;
        auto  pos  = std::find_if(objs.begin(), objs.end(), [&](auto obj){ return obj.id == e.body.id; });

        if(pos != objs.end())
        {
            e.body.success = true;
            objs.erase(pos); // TODO: replace object vector with something else so can delete without relocating everything.
        }
        else { e.body.success = false; }
    }).template on_post<e_register_object>([&](auto& e){
        auto& o = e.body.object;
        o.id = this->partial_snapshot.engine_config.last_object_id++;
        this->partial_snapshot.objects.push_back(o);
    }).template on_post<e_register_mesh>([&](auto& e){
        auto& m = e.body.mesh;
        m.id = this->partial_snapshot.engine_config.last_mesh_id++;
        meshes.push_back(m);
    }).template on_post<e_change_tps>([&](auto& e){
        this->partial_snapshot.engine_config.ticks_per_second = e.body.tps;
    }).template on_post<e_set_camera>([&](auto& e){
        this->partial_snapshot.camera_object_id = e.body.object_id;
    });

    // TODO: order messages by receiver_object_id before ticking for easier message vector management.
    //this->partial_snapshot.messageboard;

    // Then set this snapshot in stone and do the tick proper.
    this->last_snapshot    = ghuva::move(this->partial_snapshot);
    this->partial_snapshot = snapshot{};
    this->partial_snapshot.engine_config = this->last_snapshot.engine_config;

    // TODO: run me in parallel (requires engine methods like post() and message() to be thread-safe first).
    for(auto& obj : this->last_snapshot.objects) if(obj.tick) obj.on_tick(obj, dt, this->last_snapshot, *this);
}

template <typename T, typename T2>
template <typename E>
constexpr auto ghuva::engine<T, T2>::post(E&& event, u64 source_id) -> u64
{
    using Event = ghuva::event< ghuva::remove_cvref_t<E> >;

    if constexpr(engine::postboard_t::template supports<Event>)
    {
        this->partial_snapshot.postboard.template get< Event >().push_back({
            .id = this->partial_snapshot.engine_config.last_event_id++,
            .source_object_id = source_id,
            .posted_at_tick = this->last_snapshot.id, // TODO: is this correct ?
            .body = ghuva::forward<E>(event)
        });
        return this->partial_snapshot.engine_config.last_event_id - 1;
    }
    else { return 0; }
}

template <typename T, typename T2>
template <typename M>
constexpr auto ghuva::engine<T, T2>::message(M&& message, u64 target_id, u64 source_id) -> u64
{
    using Message = ghuva::message< ghuva::remove_cvref_t<M> >;

    if constexpr(engine::messageboard_t::template supports<Message>)
    {
        this->partial_snapshot.messageboard.template get< Message >().push_back({
            .id = this->partial_snapshot.engine_config.last_message_id++,
            .target_object_id = target_id,
            .source_object_id = source_id,
            .posted_at_tick = this->last_snapshot.id, // TODO: is this correct ?
            .body = ghuva::forward<M>(message)
        });
        return this->partial_snapshot.engine_config.last_event_id - 1;
    }
    else { return 0; }
}

template <typename T, typename T2>
template <typename E, typename F>
constexpr auto ghuva::engine<T, T2>::snapshot::on_post(F&& f) -> snapshot&
{
    if constexpr(postboard_t::template supports<E>)
        for(auto& e : postboard.template get< E >()) { f(e); }
    return *this;
}

template <typename T, typename T2>
template <typename E, typename F>
constexpr auto ghuva::engine<T, T2>::snapshot::on_post(F&& f) const -> snapshot const&
{
    if constexpr(postboard_t::template supports<E>)
        for(auto& e : postboard.template get< E >()) { f(e); }
    return *this;
}

template <typename T, typename T2>
template <typename F>
constexpr auto ghuva::engine<T, T2>::snapshot::all_posts(F&& f) const -> snapshot const&
{
    std::apply([&](auto& ...e){(..., f(e));}, this->postboard.regions);
    return *this;
}

