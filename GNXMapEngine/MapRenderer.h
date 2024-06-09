//
//  MapRenderer.hpp
//  GNXMapEngine
//
//  Created by zhouxuguang on 2024/6/9.
//

#ifndef MapRenderer_hpp
#define MapRenderer_hpp

#include <MetalKit/MetalKit.h>

// Our platform independent render class
@interface AAPLRenderer : NSObject

- (nonnull instancetype)initWithMetalLayer:(nonnull CAMetalLayer *)mtkLayer library:(nonnull id<MTLLibrary>)library;
- (void)drawFrame;

@end

#endif /* MapRenderer_hpp */
