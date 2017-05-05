#include <mbgl/renderer/layers/render_circle_layer.hpp>
#include <mbgl/renderer/buckets/circle_bucket.hpp>
#include <mbgl/style/layers/circle_layer_impl.hpp>
#include <mbgl/geometry/feature_index.hpp>
#include <mbgl/util/math.hpp>
#include <mbgl/util/intersection_tests.hpp>
#include <mbgl/gl/context.hpp>
#include <mbgl/renderer/render_tile.hpp>
#include <mbgl/tile/tile.hpp>

namespace mbgl {

RenderCircleLayer::RenderCircleLayer(Immutable<style::CircleLayer::Impl> _impl)
    : RenderLayer(style::LayerType::Circle, _impl) {
}

const style::CircleLayer::Impl& RenderCircleLayer::impl() const {
    return static_cast<const style::CircleLayer::Impl&>(*baseImpl);
}

std::unique_ptr<Bucket> RenderCircleLayer::createBucket(const BucketParameters& parameters, const std::vector<const RenderLayer*>& layers) const {
    return std::make_unique<CircleBucket>(parameters, layers);
}

void RenderCircleLayer::transition(const TransitionParameters& parameters) {
    unevaluated = impl().paint.transition(parameters, std::move(unevaluated));
}

void RenderCircleLayer::evaluate(const PropertyEvaluationParameters& parameters) {
    evaluated = unevaluated.evaluate(parameters);

    passes = ((evaluated.get<style::CircleRadius>().constantOr(1) > 0 ||
               evaluated.get<style::CircleStrokeWidth>().constantOr(1) > 0)
              && (evaluated.get<style::CircleColor>().constantOr(Color::black()).a > 0 ||
                  evaluated.get<style::CircleStrokeColor>().constantOr(Color::black()).a > 0)
              && (evaluated.get<style::CircleOpacity>().constantOr(1) > 0 ||
                  evaluated.get<style::CircleStrokeOpacity>().constantOr(1) > 0))
             ? RenderPass::Translucent : RenderPass::None;
}

bool RenderCircleLayer::hasTransition() const {
    return unevaluated.hasTransition();
}

void RenderCircleLayer::uploadBuckets(gl::Context& context) {
    for (const auto& tileRef : renderTiles) {
        const auto& bucket = tileRef.get().tile.getBucket(*this);
        if (bucket && bucket->needsUpload()) {
            bucket->upload(context);
        }
    }
}

void RenderCircleLayer::render(Painter& painter, PaintParameters& parameters, const RenderSource*) {
    for (auto& tileRef : renderTiles) {
        auto& tile = tileRef.get();
//        MBGL_DEBUG_GROUP(context, getID() + " - " + util::toString(tile.id));
        auto bucket = tile.tile.getBucket(*this);
        bucket->render(painter, parameters, *this, tile);
    }
}


bool RenderCircleLayer::queryIntersectsFeature(
        const GeometryCoordinates& queryGeometry,
        const GeometryTileFeature& feature,
        const float zoom,
        const float bearing,
        const float pixelsToTileUnits) const {

    // Translate query geometry
    auto translatedQueryGeometry = FeatureIndex::translateQueryGeometry(
            queryGeometry,
            evaluated.get<style::CircleTranslate>(),
            evaluated.get<style::CircleTranslateAnchor>(),
            bearing,
            pixelsToTileUnits);

    // Evaluate function
    auto circleRadius = evaluated.get<style::CircleRadius>()
                                .evaluate(feature, zoom, style::CircleRadius::defaultValue())
                        * pixelsToTileUnits;

    // Test intersection
    return util::polygonIntersectsBufferedMultiPoint(translatedQueryGeometry.value_or(queryGeometry), feature.getGeometries(), circleRadius);
}

} // namespace mbgl
