# 三维地球地平线剔除

参考：

- [Cesium: Horizon Culling](https://cesium.com/blog/2013/04/25/horizon-culling/)
- [Cesium: Computing the Horizon Occlusion Point](https://cesium.com/blog/2013/05/09/computing-the-horizon-occlusion-point/)
- [CesiumJS: EllipsoidalOccluder.js](https://github.com/CesiumGS/cesium/blob/main/packages/engine/Source/Core/EllipsoidalOccluder.js)

## 当前实现的判定范围

`HorizonCulling` 判断的是经纬度矩形对应的**连续 WGS84 零高度椭球面**，不使用瓦片中心、有限采样点或外接包围体作最终判定。用户选择了以 WGS84 椭球面为准。DEM 高低起伏不在这个判定范围内；若要求按实际 DEM 表面保证可见性，必须另有 DEM 高度范围和遮挡体约束。

将椭球的三个轴分别缩放为 1 后，椭球面点 `P` 与相机 `C` 变成单位球坐标。一个表面点在地平线之上当且仅当 `C·P >= 1`。因此整个瓦片可剔除当且仅当矩形内 `max(C·P) < 1`。代码在浮点误差范围内保留一条 `1e-12` 的可见带。

对 WGS84 旋转椭球，地理纬度 `φ` 在缩放空间变成地心纬度 `β`，满足 `tan β = (b/a) tan φ`。对经纬度矩形，`C·P` 可写成

```
Cx cosβ cosλ + Cy cosβ sinλ + Cz sinβ
```

先在经度区间取 `Cx cosλ + Cy sinλ` 的最大值，再在纬度区间取余下正弦函数的最大值。两个一维最大值只需检查区间端点和可能位于区间内部的极大值点，因此每块瓦片每帧是常数时间，瓦片的经纬度三角函数在节点创建时预计算。

## 接入和开关

四叉树先做原有 AABB 视锥剔除，再做地平线剔除；只有两项均通过才继续细化。`EarthNode` 默认开启，必要时可通过 `SetHorizonCullingEnabled(bool)` 在代码中切换。动画抓图日志新增 `horizonCulled` 累计计数。

## 可重复的数值对比

运行：

```
cmake --build build-map-native --target TestHorizonCulling
./build-map-native/TestHorizonCulling
```

测试在无窗口环境中复现四叉树的两个根、AABB 视锥剔除、分裂比例 `distance / halfBoxDiagonal < 1` 和最大 9 级限制。`requested` 是从空树开始所创建的瓦片节点数。测试还按 `TileDataSource` 的瓦片编号和路径规则，统计本机影像、地形目录中实际存在的瓦片文件数；目录可用 `GNX_MAP_IMAGE_TILES`、`GNX_MAP_TERRAIN_TILES` 覆盖。文件存在数表示这些请求能够找到本地数据，仍不等于解码和 GPU 上传完成数。

| 相机状态 | 请求节点：关→开 | 本地影像文件：关→开 | 本地地形文件：关→开 |
|---|---:|---:|---:|
| 太空，正下视 | 2→2 | 2→2 | 2→2 |
| 太空，倾斜 | 2→2 | 2→2 | 2→2 |
| 500 km，中纬正下视 | 46→38 | 46→38 | 46→38 |
| 500 km，中纬倾斜 | 46→38 | 46→38 | 46→38 |
| 10 km，中纬正下视 | 102→74 | 90→62 | 90→62 |
| 10 km，中纬倾斜 | 98→70 | 90→62 | 90→62 |
| 500 km，高纬正下视 | 98→98 | 98→98 | 98→98 |

太空场景只生成两个根瓦片，没有子瓦片请求可节省。高纬场景有被地平线拒绝的节点，但这些节点在该 LOD 条件下已经不会继续分裂，所以请求数不变。不同视角下效果取决于视锥和 LOD 规则。

数值测试还覆盖随机相机／瓦片、跨日期变更线、两极、两侧的地平线切线边界，并使用独立的相机到采样点线段与单位球相交测试检查误剔除。

本机尝试运行图形程序以采集真实 `requests/results` 时，开启和关闭地平线剔除都会在 Metal 编译 `TerrainPayload` 的 fragment shader 阶段报 `threadgroup ... cannot be declared in a fragment function`，未得到稳定的运行时加载完成计数。表中数字是可重复的无窗口四叉树请求量对比，不代表已成功解码或上传的瓦片数。
