#ifndef GNX_MAP_ENGINE_QUADTILEID_DSGKDFMGF_INCLUDE_GJGJDF
#define GNX_MAP_ENGINE_QUADTILEID_DSGKDFMGF_INCLUDE_GJGJDF
#endif // !GNX_MAP_ENGINE_QUADTILEID_DSGKDFMGF_INCLUDE_GJGJDF

#include "EarthEngineDefine.h"

EARTH_CORE_NAMESPACE_BEGIN

// ��ƬID����
class QuadTileID
{
public:
	/**
	* @brief ����һ����ʵ��
	*
	*/
	constexpr QuadTileID() noexcept
        : level(0), x(0), y(0)
	{
	}

    /**
    * @brief ����һ����ʵ��
    *
    * @param level_ �ڵ�Ĳ㼶��0�Ǹ��ڵ�
    * @param x_ ��Ƭ��x����
    * @param y_ ��Ƭ��y����
    */
    constexpr QuadTileID(uint32_t level_, uint32_t x_, uint32_t y_) noexcept
        : level(level_), x(x_), y(y_) 
    {
    }

    /**
     * @brief �ж�������ƬID�Ƿ����
     */
    constexpr bool operator==(const QuadTileID& other) const noexcept 
    {
        return this->level == other.level && this->x == other.x &&
            this->y == other.y;
    }

    /**
     * @brief �ж�������ƬID�Ƿ����
     */
    constexpr bool operator!=(const QuadTileID& other) const noexcept 
    {
        return !(*this == other);
    }

    /**
     * @brief ���ظ��ڵ����ƬID
     *
     * �����0���ڵ㣬��������
     *
     * @return ���ڵ���ƬID
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
     * @brief ��Ƭ�㼶��0�Ǹ��ڵ�
     */
    uint32_t level;

    /**
     * @brief ��ƬID��x����
     */
    uint32_t x;

    /**
     * @brief ��ƬID��y����
     */
    uint32_t y;
};

QuadTileID GetTileID(uint32_t level, double rLong, double rLat);

EARTH_CORE_NAMESPACE_END

