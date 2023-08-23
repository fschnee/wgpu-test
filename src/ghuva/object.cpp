#include "object.hpp"

ghuva::object::object(constructorargs&& args)
    : name{ ghuva::move(args.name) }
    , draw{ args.draw }
    , tick{ args.tick }
    , mesh{ ghuva::move(args.mesh) }
    , on_tick{ ghuva::move(args.on_tick) }
    , world_transform{ ghuva::move(args.world_transform) }
    , model_transform{ ghuva::move(args.model_transform) }
{}

auto ghuva::object::look_at(fpoint const& _target) -> object&
{
    [[maybe_unused]] const auto target = _target - this->mt().pos;
    //TODO: Implementar. Só fazer this->mt().rot apontar target.

    return *this;
}
