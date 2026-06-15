#include "user_ui_route_map.h"

#include <stdbool.h>
#include <stddef.h>

#include "userapp_race.h"
#include "userlib_oled.h"

#define USER_UI_ROUTE_MAP_ORIGIN_X 0u
#define USER_UI_ROUTE_MAP_ORIGIN_Y 0u
#define USER_UI_ROUTE_MAP_WIDTH 59u
#define USER_UI_ROUTE_MAP_HEIGHT 56u

typedef enum
{
    USER_UI_ROUTE_PRIMITIVE_LINE = 0,
    USER_UI_ROUTE_PRIMITIVE_RECT,
    USER_UI_ROUTE_PRIMITIVE_ARC
} USER_UI_RoutePrimitiveType_t;

typedef struct
{
    USER_UI_RoutePrimitiveType_t type;
    uint8_t x1;
    uint8_t y1;
    uint8_t x2;
    uint8_t y2;
    uint16_t arg1;
    uint16_t arg2;
} USER_UI_RoutePrimitive_t;

typedef struct
{
    uint8_t route;
    const USER_UI_RoutePrimitive_t *primitives;
    uint8_t primitive_count;
} USER_UI_RouteMapDef_t;

#define USER_UI_ROUTE_LINE(x1_, y1_, x2_, y2_) \
    {USER_UI_ROUTE_PRIMITIVE_LINE, (x1_), (y1_), (x2_), (y2_), 0u, 0u}

#define USER_UI_ROUTE_RECT(x1_, y1_, x2_, y2_) \
    {USER_UI_ROUTE_PRIMITIVE_RECT, (x1_), (y1_), (x2_), (y2_), 0u, 0u}

#define USER_UI_ROUTE_ARC(cx_, cy_, radius_, start_, end_) \
    {USER_UI_ROUTE_PRIMITIVE_ARC, (cx_), (cy_), (radius_), 0u, \
     (start_), (end_)}

/*
 * Text row 0 uses GRAM page 0. Pixel drawing maps y=56..63 to the same page,
 * so route graphics must remain within pixel y=0..55 to protect the title.
 */
static const USER_UI_RoutePrimitive_t user_ui_template_route_map[] = {
    USER_UI_ROUTE_RECT(0u, 0u, 58u, 55u),

    USER_UI_ROUTE_LINE(17u, 13u, 17u, 42u),
    USER_UI_ROUTE_LINE(41u, 13u, 41u, 42u),
    USER_UI_ROUTE_ARC(29u, 13u, 12u, 0u, 180u),
    USER_UI_ROUTE_ARC(29u, 42u, 12u, 180u, 360u),

    USER_UI_ROUTE_LINE(17u, 25u, 17u, 31u),
    USER_UI_ROUTE_LINE(17u, 31u, 14u, 28u),
    USER_UI_ROUTE_LINE(17u, 31u, 20u, 28u),

    USER_UI_ROUTE_LINE(41u, 31u, 41u, 25u),
    USER_UI_ROUTE_LINE(41u, 25u, 38u, 28u),
    USER_UI_ROUTE_LINE(41u, 25u, 44u, 28u),
};

static const USER_UI_RoutePrimitive_t user_ui_arc_test_route_map[] = {
    USER_UI_ROUTE_RECT(0u, 0u, 58u, 55u),

    USER_UI_ROUTE_ARC(25u, 30u, 16u, 270u, 360u),
    USER_UI_ROUTE_LINE(25u, 14u, 31u, 14u),
    USER_UI_ROUTE_LINE(31u, 14u, 28u, 11u),
    USER_UI_ROUTE_LINE(31u, 14u, 28u, 17u),

    USER_UI_ROUTE_ARC(36u, 19u, 8u, 90u, 180u),
    USER_UI_ROUTE_LINE(36u, 27u, 36u, 42u),
    USER_UI_ROUTE_LINE(36u, 42u, 33u, 39u),
    USER_UI_ROUTE_LINE(36u, 42u, 39u, 39u),

    USER_UI_ROUTE_ARC(36u, 42u, 8u, 0u, 180u),
};

static const USER_UI_RouteMapDef_t user_ui_route_maps[] = {
    {
        RACE_ROUTE_TEMPLATE,
        user_ui_template_route_map,
        (uint8_t)(sizeof(user_ui_template_route_map) /
                  sizeof(user_ui_template_route_map[0])),
    },
    {
        RACE_ROUTE_ARC_TEST,
        user_ui_arc_test_route_map,
        (uint8_t)(sizeof(user_ui_arc_test_route_map) /
                  sizeof(user_ui_arc_test_route_map[0])),
    },
};

static bool USER_UI_RouteMap_PointFits(uint8_t x, uint8_t y)
{
    return (x < USER_UI_ROUTE_MAP_WIDTH) &&
           (y < USER_UI_ROUTE_MAP_HEIGHT);
}

static bool USER_UI_RouteMap_ArcFits(const USER_UI_RoutePrimitive_t *primitive)
{
    uint16_t radius = primitive->x2;

    return (primitive->x1 >= radius) &&
           (primitive->y1 >= radius) &&
           (((uint16_t)primitive->x1 + radius) < USER_UI_ROUTE_MAP_WIDTH) &&
           (((uint16_t)primitive->y1 + radius) < USER_UI_ROUTE_MAP_HEIGHT);
}

static void USER_UI_RouteMap_DrawPrimitive(
    const USER_UI_RoutePrimitive_t *primitive)
{
    uint8_t x1;
    uint8_t y1;
    uint8_t x2;
    uint8_t y2;

    if (primitive == NULL)
    {
        return;
    }

    x1 = (uint8_t)(USER_UI_ROUTE_MAP_ORIGIN_X + primitive->x1);
    y1 = (uint8_t)(USER_UI_ROUTE_MAP_ORIGIN_Y + primitive->y1);
    x2 = (uint8_t)(USER_UI_ROUTE_MAP_ORIGIN_X + primitive->x2);
    y2 = (uint8_t)(USER_UI_ROUTE_MAP_ORIGIN_Y + primitive->y2);

    switch (primitive->type)
    {
    case USER_UI_ROUTE_PRIMITIVE_LINE:
        if (USER_UI_RouteMap_PointFits(primitive->x1, primitive->y1) &&
            USER_UI_RouteMap_PointFits(primitive->x2, primitive->y2))
        {
            USER_OLED_DrawLine(x1, y1, x2, y2);
        }
        break;

    case USER_UI_ROUTE_PRIMITIVE_RECT:
        if (USER_UI_RouteMap_PointFits(primitive->x1, primitive->y1) &&
            USER_UI_RouteMap_PointFits(primitive->x2, primitive->y2))
        {
            USER_OLED_DrawRect(x1, y1, x2, y2, false);
        }
        break;

    case USER_UI_ROUTE_PRIMITIVE_ARC:
        if (USER_UI_RouteMap_ArcFits(primitive))
        {
            USER_OLED_DrawArc(x1,
                              y1,
                              primitive->x2,
                              primitive->arg1,
                              primitive->arg2);
        }
        break;

    default:
        break;
    }
}

void USER_UI_RouteMap_Draw(uint8_t route)
{
    uint8_t map_index;

    for (map_index = 0u;
         map_index < (uint8_t)(sizeof(user_ui_route_maps) /
                               sizeof(user_ui_route_maps[0]));
         map_index++)
    {
        const USER_UI_RouteMapDef_t *map = &user_ui_route_maps[map_index];
        uint8_t primitive_index;

        if (map->route != route)
        {
            continue;
        }

        for (primitive_index = 0u;
             primitive_index < map->primitive_count;
             primitive_index++)
        {
            USER_UI_RouteMap_DrawPrimitive(
                &map->primitives[primitive_index]);
        }
        return;
    }
}
