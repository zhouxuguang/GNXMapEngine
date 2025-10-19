#ifndef GNX_MAP_ENGINE_QUADTILEID_DSGKDFMGF_INCLUDE_GJGJDF
#define GNX_MAP_ENGINE_QUADTILEID_DSGKDFMGF_INCLUDE_GJGJDF

#include "EarthEngineDefine.h"

EARTH_CORE_NAMESPACE_BEGIN

// 瓦片ID的类
class QuadTileID
{
public:
	/**
	* @brief 创建一个新实例
	*
	*/
	constexpr QuadTileID() noexcept
        : level(0), x(0), y(0)
	{
	}

    /**
    * @brief 创建一个新实例
    *
    * @param level_ 节点的层级，0是根节点
    * @param x_ 瓦片的x坐标
    * @param y_ 瓦片的y坐标
    */
    constexpr QuadTileID(uint32_t level_, uint32_t x_, uint32_t y_) noexcept
        : level(level_), x(x_), y(y_) 
    {
    }

    /**
     * @brief 判断两个瓦片ID是否相等
     */
    constexpr bool operator==(const QuadTileID& other) const noexcept 
    {
        return this->level == other.level && this->x == other.x &&
            this->y == other.y;
    }

    /**
     * @brief 判断两个瓦片ID是否不相等
     */
    constexpr bool operator!=(const QuadTileID& other) const noexcept 
    {
        return !(*this == other);
    }

    /**
     * @brief 返回父节点的瓦片ID
     *
     * 如果是0级节点，返回自身
     *
     * @return 父节点瓦片ID
     */
    constexpr QuadTileID GetParent() const noexcept 
    {
        if (this->level == 0) 
        {
            return *this;
        }
        return QuadTileID(this->level - 1, this->x >> 1, this->y >> 1);
    }

    /**
     * @brief 瓦片层级，0是根节点
     */
    uint32_t level;

    /**
     * @brief 瓦片ID的x坐标
     */
    uint32_t x;

    /**
     * @brief 瓦片ID的y坐标
     */
    uint32_t y;
};


// 根据经纬度和级别创建瓦片ID
QuadTileID GetTileID(uint32_t level, double rLong, double rLat);

EARTH_CORE_NAMESPACE_END

#endif // !GNX_MAP_ENGINE_QUADTILEID_DSGKDFMGF_INCLUDE_GJGJDF

