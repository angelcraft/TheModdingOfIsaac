#pragma once
struct WorkshopVector {
    float x;
    float y;
};

static const char kWorkshopVectorMetatable[] = "TheModdingOfIsaac.Vector";

WorkshopVector* workshop_vector_check(lua_State* state, int index) {
    return static_cast<WorkshopVector*>(
        luaL_checkudata(state, index, kWorkshopVectorMetatable));
}

int workshop_vector_push(lua_State* state, float x, float y) {
    WorkshopVector* value = static_cast<WorkshopVector*>(
        lua_newuserdata(state, sizeof(WorkshopVector)));
    value->x = x;
    value->y = y;
    luaL_getmetatable(state, kWorkshopVectorMetatable);
    lua_setmetatable(state, -2);
    return 1;
}

int workshop_vector_new(lua_State* state) {
    // A callable Lua table is passed as argument 1 to its __call handler.
    const int x_index = lua_istable(state, 1) ? 2 : 1;
    const float x = static_cast<float>(luaL_checknumber(state, x_index));
    const float y = static_cast<float>(luaL_checknumber(state, x_index + 1));
    return workshop_vector_push(state, x, y);
}

int workshop_vector_from_angle(lua_State* state) {
    const int angle_index = lua_istable(state, 1) ? 2 : 1;
    const float degrees = static_cast<float>(luaL_checknumber(state, angle_index));
    const float radians = degrees * 0.01745329251994329577f;
    return workshop_vector_push(state, std::cos(radians), std::sin(radians));
}

int workshop_vector_index(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const char* key = luaL_checkstring(state, 2);
    if (std::strcmp(key, "X") == 0) lua_pushnumber(state, value->x);
    else if (std::strcmp(key, "Y") == 0) lua_pushnumber(state, value->y);
    else {
        luaL_getmetatable(state, kWorkshopVectorMetatable);
        lua_getfield(state, -1, key);
        lua_remove(state, -2);
    }
    return 1;
}

int workshop_vector_newindex(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const char* key = luaL_checkstring(state, 2);
    const float number = static_cast<float>(luaL_checknumber(state, 3));
    if (std::strcmp(key, "X") == 0) value->x = number;
    else if (std::strcmp(key, "Y") == 0) value->y = number;
    else return luaL_error(state, "Vector only exposes mutable X and Y components");
    return 0;
}

int workshop_vector_add(lua_State* state) {
    WorkshopVector* a = workshop_vector_check(state, 1);
    WorkshopVector* b = workshop_vector_check(state, 2);
    return workshop_vector_push(state, a->x + b->x, a->y + b->y);
}
int workshop_vector_sub(lua_State* state) {
    WorkshopVector* a = workshop_vector_check(state, 1);
    WorkshopVector* b = workshop_vector_check(state, 2);
    return workshop_vector_push(state, a->x - b->x, a->y - b->y);
}
int workshop_vector_unm(lua_State* state) {
    WorkshopVector* a = workshop_vector_check(state, 1);
    return workshop_vector_push(state, -a->x, -a->y);
}
int workshop_vector_mul(lua_State* state) {
    WorkshopVector* value = nullptr;
    lua_Number scalar = 0;
    if (lua_isuserdata(state, 1)) {
        value = workshop_vector_check(state, 1);
        scalar = luaL_checknumber(state, 2);
    } else {
        scalar = luaL_checknumber(state, 1);
        value = workshop_vector_check(state, 2);
    }
    return workshop_vector_push(state, value->x * static_cast<float>(scalar),
                               value->y * static_cast<float>(scalar));
}
int workshop_vector_div(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const lua_Number divisor = luaL_checknumber(state, 2);
    if (divisor == 0) return luaL_error(state, "Vector division by zero");
    return workshop_vector_push(state, value->x / static_cast<float>(divisor),
                               value->y / static_cast<float>(divisor));
}
int workshop_vector_eq(lua_State* state) {
    WorkshopVector* a = workshop_vector_check(state, 1);
    WorkshopVector* b = workshop_vector_check(state, 2);
    lua_pushboolean(state, a->x == b->x && a->y == b->y);
    return 1;
}
int workshop_vector_tostring(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    char text[96] = {};
    std::snprintf(text, sizeof(text), "Vector(%g,%g)", value->x, value->y);
    lua_pushstring(state, text);
    return 1;
}
int workshop_vector_length_squared(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    lua_pushnumber(state, value->x * value->x + value->y * value->y);
    return 1;
}
int workshop_vector_length(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    lua_pushnumber(state, std::sqrt(value->x * value->x + value->y * value->y));
    return 1;
}
int workshop_vector_normalized(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const float length = std::sqrt(value->x * value->x + value->y * value->y);
    if (length == 0) return workshop_vector_push(state, 0, 0);
    return workshop_vector_push(state, value->x / length, value->y / length);
}
int workshop_vector_normalize(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const float length = std::sqrt(value->x * value->x + value->y * value->y);
    if (length != 0) { value->x /= length; value->y /= length; }
    return 0;
}
int workshop_vector_resized(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const float length = std::sqrt(value->x * value->x + value->y * value->y);
    const float target = static_cast<float>(luaL_checknumber(state, 2));
    if (length == 0) return workshop_vector_push(state, 0, 0);
    return workshop_vector_push(state, value->x * target / length,
                               value->y * target / length);
}
int workshop_vector_resize(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const float length = std::sqrt(value->x * value->x + value->y * value->y);
    const float target = static_cast<float>(luaL_checknumber(state, 2));
    if (length != 0) { value->x *= target / length; value->y *= target / length; }
    return 0;
}
int workshop_vector_rotated(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const float radians = static_cast<float>(luaL_checknumber(state, 2)) *
                         0.01745329251994329577f;
    const float c = std::cos(radians), s = std::sin(radians);
    return workshop_vector_push(state, value->x * c - value->y * s,
                               value->x * s + value->y * c);
}
int workshop_vector_angle(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    lua_pushnumber(state, std::atan2(value->y, value->x) * 57.29577951308232);
    return 1;
}
int workshop_vector_dot(lua_State* state) {
    WorkshopVector* a = workshop_vector_check(state, 1);
    WorkshopVector* b = workshop_vector_check(state, 2);
    lua_pushnumber(state, a->x * b->x + a->y * b->y);
    return 1;
}
int workshop_vector_cross(lua_State* state) {
    WorkshopVector* a = workshop_vector_check(state, 1);
    WorkshopVector* b = workshop_vector_check(state, 2);
    lua_pushnumber(state, a->x * b->y - a->y * b->x);
    return 1;
}
int workshop_vector_distance_squared(lua_State* state) {
    const int first = lua_istable(state, 1) ? 2 : 1;
    WorkshopVector* a = workshop_vector_check(state, first);
    WorkshopVector* b = workshop_vector_check(state, first + 1);
    const float x = a->x - b->x, y = a->y - b->y;
    lua_pushnumber(state, x * x + y * y);
    return 1;
}
int workshop_vector_distance(lua_State* state) {
    const int first = lua_istable(state, 1) ? 2 : 1;
    WorkshopVector* a = workshop_vector_check(state, first);
    WorkshopVector* b = workshop_vector_check(state, first + 1);
    const float x = a->x - b->x, y = a->y - b->y;
    lua_pushnumber(state, std::sqrt(x * x + y * y));
    return 1;
}
int workshop_vector_lerp(lua_State* state) {
    const int first = lua_istable(state, 1) ? 2 : 1;
    WorkshopVector* a = workshop_vector_check(state, first);
    WorkshopVector* b = workshop_vector_check(state, first + 1);
    const float t = static_cast<float>(luaL_checknumber(state, first + 2));
    return workshop_vector_push(state, a->x + (b->x - a->x) * t,
                               a->y + (b->y - a->y) * t);
}
int workshop_vector_clamped(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const float min_x = static_cast<float>(luaL_checknumber(state, 2));
    const float min_y = static_cast<float>(luaL_checknumber(state, 3));
    const float max_x = static_cast<float>(luaL_checknumber(state, 4));
    const float max_y = static_cast<float>(luaL_checknumber(state, 5));
    const float x = value->x < min_x ? min_x : (value->x > max_x ? max_x : value->x);
    const float y = value->y < min_y ? min_y : (value->y > max_y ? max_y : value->y);
    return workshop_vector_push(state, x, y);
}
int workshop_vector_clamp(lua_State* state) {
    WorkshopVector* value = workshop_vector_check(state, 1);
    const float min_x = static_cast<float>(luaL_checknumber(state, 2));
    const float min_y = static_cast<float>(luaL_checknumber(state, 3));
    const float max_x = static_cast<float>(luaL_checknumber(state, 4));
    const float max_y = static_cast<float>(luaL_checknumber(state, 5));
    value->x = value->x < min_x ? min_x : (value->x > max_x ? max_x : value->x);
    value->y = value->y < min_y ? min_y : (value->y > max_y ? max_y : value->y);
    return 0;
}

void register_vector_api(lua_State* state) {
    luaL_newmetatable(state, kWorkshopVectorMetatable);
    lua_pushcfunction(state, workshop_vector_index); lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, workshop_vector_newindex); lua_setfield(state, -2, "__newindex");
    lua_pushcfunction(state, workshop_vector_add); lua_setfield(state, -2, "__add");
    lua_pushcfunction(state, workshop_vector_sub); lua_setfield(state, -2, "__sub");
    lua_pushcfunction(state, workshop_vector_mul); lua_setfield(state, -2, "__mul");
    lua_pushcfunction(state, workshop_vector_div); lua_setfield(state, -2, "__div");
    lua_pushcfunction(state, workshop_vector_unm); lua_setfield(state, -2, "__unm");
    lua_pushcfunction(state, workshop_vector_eq); lua_setfield(state, -2, "__eq");
    lua_pushcfunction(state, workshop_vector_tostring); lua_setfield(state, -2, "__tostring");
    lua_pushcfunction(state, workshop_vector_length); lua_setfield(state, -2, "Length");
    lua_pushcfunction(state, workshop_vector_length_squared); lua_setfield(state, -2, "LengthSquared");
    lua_pushcfunction(state, workshop_vector_normalized); lua_setfield(state, -2, "Normalized");
    lua_pushcfunction(state, workshop_vector_normalize); lua_setfield(state, -2, "Normalize");
    lua_pushcfunction(state, workshop_vector_resized); lua_setfield(state, -2, "Resized");
    lua_pushcfunction(state, workshop_vector_resize); lua_setfield(state, -2, "Resize");
    lua_pushcfunction(state, workshop_vector_rotated); lua_setfield(state, -2, "Rotated");
    lua_pushcfunction(state, workshop_vector_angle); lua_setfield(state, -2, "GetAngleDegrees");
    lua_pushcfunction(state, workshop_vector_dot); lua_setfield(state, -2, "Dot");
    lua_pushcfunction(state, workshop_vector_cross); lua_setfield(state, -2, "Cross");
    lua_pushcfunction(state, workshop_vector_distance); lua_setfield(state, -2, "Distance");
    lua_pushcfunction(state, workshop_vector_distance_squared); lua_setfield(state, -2, "DistanceSquared");
    lua_pushcfunction(state, workshop_vector_lerp); lua_setfield(state, -2, "Lerp");
    lua_pushcfunction(state, workshop_vector_clamped); lua_setfield(state, -2, "Clamped");
    lua_pushcfunction(state, workshop_vector_clamp); lua_setfield(state, -2, "Clamp");
    lua_pop(state, 1);

    lua_newtable(state);
    lua_pushcfunction(state, workshop_vector_from_angle);
    lua_setfield(state, -2, "FromAngle");
    lua_pushcfunction(state, workshop_vector_distance);
    lua_setfield(state, -2, "Distance");
    lua_pushcfunction(state, workshop_vector_distance_squared);
    lua_setfield(state, -2, "DistanceSquared");
    lua_pushcfunction(state, workshop_vector_lerp);
    lua_setfield(state, -2, "Lerp");
    lua_newtable(state);
    lua_pushcfunction(state, workshop_vector_new);
    lua_setfield(state, -2, "__call");
    lua_setmetatable(state, -2);
    lua_setglobal(state, "Vector");
}
