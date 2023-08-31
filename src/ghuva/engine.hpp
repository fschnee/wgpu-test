#pragma once

#include "utils/guarded.hpp"
#include "utils/chrono.hpp"
#include "utils/aliases.hpp"
#include "utils/forward.hpp"
#include "object.hpp"
#include "mesh.hpp"

#include <shared_mutex>
#include <vector>
#include <mutex>
#include <tuple>

#include <fmt/core.h>

#include <omp.h>

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
    // NOTE: If parallel_ticking == false this will already be deterministic.
    //       However, if parallel_ticking we want to have each thread have its own postboard
    //       and at the end of ticking we merge them all sequentially.
    //       Basically, tick in chunks, keep a chunk id, then order based on chunkid THEN messageid
    //       and use that final ordering to produce the final id.
    template <typename T>
    struct message
    {
        using body_t = T;
        u64 id;
        u64 target_object_id;
        u64 source_object_id; // 0 = engine/external/unidentified events.
        u64 posted_at_tick;
        body_t body;

    private:
        // TODO: we can refactor this out of here probably.
        //u64 chunk_id;
        //u64 chunk_message_id;
        //friend class engine;
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

        f32 time_multiplier = 1.0f;

        f32 total_time = 0.0f;

        bool parallel_ticking = true;

        u64 last_mesh_id = 1;
        u64 last_object_id = 1;
        u64 last_event_id = 1;
        u64 last_message_id = 1;
    };

    // All times in seconds.
    struct engine_perf
    {
        f32 fixed_tick; // Time for fixed_tick().
        f32 commit; // Time it took to commit this snapshot.

        f32 copy_objects; // Time it takes to copy from last_snapshot.

        f32 engine_events; // Time it takes to handle all engine events.
        // Event times.
        f32 delete_objects;
        f32 register_objects;
        f32 register_meshes;

        f32 object_ticks;
    };

    template <typename ExtraPostboardEvents, typename Messages>
    struct engine
    {
        using object_t = ghuva::object<engine>; // CRTP this bitch.

        // Some engine events.
        struct register_mesh        { ghuva::mesh mesh; /* Id is overriden */ };
        struct register_object      { object_t object; /* Id is overriden. */ };
        struct delete_object        { u64 id; bool success; };
        struct set_tps              { f32 tps; };
        struct set_camera           { u64 object_id; };
        struct set_time_multiplier  { f32 time_multiplier; };
        struct set_parallel_ticking { bool parallel_ticking; };
        using  e_register_mesh        = ghuva::event< register_mesh >;
        using  e_register_object      = ghuva::event< register_object >;
        using  e_delete_object        = ghuva::event< delete_object >;
        using  e_set_tps              = ghuva::event< set_tps >;
        using  e_set_camera           = ghuva::event< set_camera >;
        using  e_set_time_multiplier  = ghuva::event< set_time_multiplier >;
        using  e_set_parallel_ticking = ghuva::event< set_parallel_ticking >;

        using default_events = impl::type_list<
            e_register_mesh,
            e_register_object,
            e_delete_object,
            e_set_tps,
            e_set_camera,
            e_set_time_multiplier,
            e_set_parallel_ticking
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
            ghuva::engine_perf   engine_perf;

            std::vector<mesh> meshes;

        private:
            // Messages are private, no looksies.
            messageboard_t messageboard;

            // Also, this is only to be used by the engine.
            template <typename E, typename F> constexpr auto on_post(F&& f) -> snapshot&;

            // Except for you, you're ok.
            // Needs access to reorder the messageboard by target_object_id.
            // This way, the messages for each object and message type are contiguous
            // in memory and we can build a view from them.
            friend class engine;
        };

        constexpr auto take_snapshot() -> snapshot;
        constexpr auto tick(f32 real_dt) -> u64;

        // Posts events to the next snapshot's postboard. Returns the event id.
        // Remember that an id = 0 means failed.
        template <typename E>
        constexpr auto post(E&& event, u64 source_id = 0) -> u64;
        // TODO: Sends a direct event to the object for next snapshot.
        template <typename E>
        constexpr auto message(E&& event, u64 target_id, u64 source_id = 0) -> u64;

    private:
        // TODO: snapshot keepalive system ?
        guarded<snapshot> last_snapshot;
        guarded<snapshot> partial_snapshot; // Postboard of this one and
                                            // other stuff are filled in
                                            // as we tick the current snapshot.

        constexpr auto fixed_tick(f32 dt) -> void;
    };

    using default_engine = engine< impl::type_list<>, impl::type_list<> >;
}

// Impls.

template <typename T, typename T2>
constexpr auto ghuva::engine<T, T2>::take_snapshot() -> snapshot
{
    snapshot temp;

    last_snapshot.read([&](auto const& s){
        temp = s;
    });

    return temp;
}

template <typename T, typename T2>
constexpr auto ghuva::engine<T, T2>::tick(f32 real_dt) -> u64
{
    f32 dt;
    f32 leftover_tick_seconds;

    partial_snapshot.write([&](auto& p) {
        dt = real_dt * p.engine_config.time_multiplier;
        p.engine_config.leftover_tick_seconds += dt;
        leftover_tick_seconds = p.engine_config.leftover_tick_seconds;
    });

    u64 start_tick;
    last_snapshot.read([&](auto const& l){
        start_tick = l.id;
    });

    while(true)
    {
        auto seconds_per_tick = last_snapshot.read([&](auto const& l){
            return 1.0f / l.engine_config.ticks_per_second;
        });
        if(leftover_tick_seconds < seconds_per_tick) break;

        this->fixed_tick(seconds_per_tick);

        partial_snapshot.write([&](auto& p){
            p.engine_config.leftover_tick_seconds -= seconds_per_tick;
        });
        leftover_tick_seconds -= seconds_per_tick;
    }

    u64 last_id;
    last_snapshot.read([&](auto const& l){
        last_id = l.id;
    });
    return last_id - start_tick;
}

template <typename T, typename T2>
constexpr auto ghuva::engine<T, T2>::fixed_tick(f32 dt) -> void
{
    auto fixed_tick_stopwatch = ghuva::chrono::stopwatch();
    f32 copy_objects_time;

    u64                   last_snapshot_id;
    std::vector<object_t> last_snapshot_objects;
    u64                   last_snapshot_camera_object_id;
    f32                   last_snapshot_total_time;
    std::vector<mesh>     last_snapshot_meshes;

    last_snapshot.read([&](auto const& l){
        last_snapshot_id          = l.id;
        copy_objects_time = ghuva::chrono::time([&]{
            last_snapshot_objects = l.objects;
        });
        last_snapshot_camera_object_id = l.camera_object_id;
        last_snapshot_total_time       = l.engine_config.total_time;
        last_snapshot_meshes           = l.meshes;
    });

    snapshot temp; // Used just before ticking.

    partial_snapshot.write([&](auto& p){
        p.engine_config.total_time = last_snapshot_total_time + dt;
        p.id                       = last_snapshot_id + 1;
        p.engine_perf.copy_objects = copy_objects_time + ghuva::chrono::time([&]{
            p.objects              = ghuva::move(last_snapshot_objects);
        });
        p.camera_object_id         = last_snapshot_camera_object_id;
        p.meshes                   = ghuva::move(last_snapshot_meshes);

        // Run the engine event handlers.
        auto engine_events_stopwatch = ghuva::chrono::stopwatch();
        p.engine_perf.delete_objects = ghuva::chrono::time([&]{
            p.template on_post<e_delete_object>([&](auto& e){
                auto& objs = p.objects;
                auto  pos  = std::find_if(objs.begin(), objs.end(), [&](auto obj){ return obj.id == e.body.id; });

                if(pos != objs.end())
                {
                    e.body.success = true;
                    objs.erase(pos); // TODO: replace object vector with something else so can delete without relocating everything.
                }
                else { e.body.success = false; }
            });
        });
        p.engine_perf.register_objects = ghuva::chrono::time([&]{
            p.template on_post<e_register_object>([&](auto& e){
                auto& o = e.body.object;
                o.id = p.engine_config.last_object_id++;
                p.objects.push_back(o);
            });
        });
        p.engine_perf.register_meshes = ghuva::chrono::time([&]{
            p.template on_post<e_register_mesh>([&](auto& e){
                auto& m = e.body.mesh;
                m.id = p.engine_config.last_mesh_id++;
                p.meshes.push_back(m);
            });
        });
        p.template on_post<e_set_tps>([&](auto& e){
            p.engine_config.ticks_per_second = e.body.tps;
        });
        p.template on_post<e_set_camera>([&](auto& e){
            p.camera_object_id = e.body.object_id;
        });
        p.template on_post<e_set_time_multiplier>([&](auto& e){
            p.engine_config.time_multiplier = e.body.time_multiplier;
        });
        p.template on_post<e_set_parallel_ticking>([&](auto& e){
            p.engine_config.parallel_ticking = e.body.parallel_ticking;
        });
        p.engine_perf.engine_events = engine_events_stopwatch.click().last_segment();

        // TODO: order messages by receiver_object_id before ticking for easier message vector management.
        //p.messageboard;

        temp = ghuva::move(p);
        p = snapshot{};
        p.engine_config = temp.engine_config;
    });

    // Do the tick proper.
    temp.engine_perf.object_ticks = ghuva::chrono::time([&]{
        // TODO: omp overhead too big. Threadpool with batching ?
        if(temp.engine_config.parallel_ticking)
        {
            #pragma omp parallel for
            for(auto& obj : temp.objects) if(obj.tick) obj.on_tick(obj, dt, temp, *this);
        }
        else
        {
            for(auto& obj : temp.objects) if(obj.tick) obj.on_tick(obj, dt, temp, *this);
        }
    });

    // Then set this snapshot in stone.
    last_snapshot.write([&](auto& l){
        auto commit_stopwatch    = ghuva::chrono::stopwatch();
        l                        = ghuva::move(temp);
        l.engine_perf.commit     = commit_stopwatch.click().last_segment();
        l.engine_perf.fixed_tick = fixed_tick_stopwatch.click().last_segment();
    });
}

template <typename T, typename T2>
template <typename E>
constexpr auto ghuva::engine<T, T2>::post(E&& event, u64 source_id) -> u64
{
    using Event = ghuva::event< ghuva::remove_cvref_t<E> >;

    if constexpr(engine::postboard_t::template supports<Event>)
    {
        u64 last_tick_id;
        last_snapshot.read([&](auto const& l) {
            last_tick_id = l.id;
        });

        u64 ret;
        partial_snapshot.write([&](auto& p){
            p.postboard.template get< Event >().push_back({
                .id = p.engine_config.last_event_id,
                .source_object_id = source_id,
                .posted_at_tick = last_tick_id,
                .body = ghuva::forward<E>(event)
            });
            ret = p.engine_config.last_event_id++;
        });
        return ret;
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
        u64 last_id;
        last_snapshot.read([&](auto const& l){
            last_id = l.id;
        });

        u64 ret;
        partial_snapshot.write([&](auto& p){
            p.messageboard.template get< Message >().push_back({
                .id = p.engine_config.last_message_id,
                .target_object_id = target_id,
                .source_object_id = source_id,
                .posted_at_tick = last_id,
                .body = ghuva::forward<M>(message)
            });
            ret = p.engine_config.last_message_id++;
        });
        return ret;
    }
    else { return 0; }
}

// This is thread-safe as long as it's always called only by fixed_tick.
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

