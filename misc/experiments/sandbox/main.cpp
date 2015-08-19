#include "MainDialog.h"

#include "../common/bouncing.h"
#include "../common/paths.h"
#include "../common/timing.h"

#include <agge/renderer_parallel.h>
#include <agge/blenders_simd.h>

#include <aggx/rasterizer.h>
#include <aggx/blenders.h>

#include <aggx/aggx_conv_stroke.h>
#include <aggx/aggx_ellipse.h>

#include <agg_conv_stroke.h>
#include <agg_rasterizer_sl_clip.h>
#include <agg_ellipse.h>
#include <agg_pixfmt_rgba.h>
#include <agg_renderer_base.h>
#include <agg_scanline_u.h>
#include <agg_rasterizer_scanline_aa.h>
#include <agg_renderer_scanline.h>

using namespace std;
using namespace demo;

const int c_thread_count = 1;
const bool c_use_original_agg = false;
const int c_balls_number = 0;//700;
typedef agge::simd::blender_solid_color blender_used;

namespace
{
	agge::simd::blender_solid_color::pixel make_pixel(aggx::rgba8 color)
	{
		agge::simd::blender_solid_color::pixel p = { color.b, color.g, color.r, 0 };
		return p;
	}

	template <typename BlenderT>
	class blender : public BlenderT
	{
	public:
		blender(aggx::rgba8 color)
			: BlenderT(make_pixel(color), color.a)
		{	}
	};


	class bitmap_rendering_buffer
	{
	public:
		typedef unsigned int pixel_type;
		typedef agg::const_row_info<pixel_type> row_data;

	public:
		bitmap_rendering_buffer(::bitmap &target)
			: _target(target)
		{	}

		pixel_type *row_ptr(int, int y, int)
		{	return reinterpret_cast<pixel_type *>(_target.row_ptr(y));	}

		unsigned int width() const
		{	return _target.width();	}

		unsigned int height() const
		{	return _target.height();	}

	private:
		::bitmap &_target;
	};



	class agg_drawer : public Drawer
	{
	public:
		typedef blender<blender_used> solid_color_brush;
		typedef agg::pixfmt_alpha_blend_rgba<agg::blender_bgra32, bitmap_rendering_buffer> pixfmt;
		typedef agg::rgba8 color_type;
		typedef agg::order_bgra component_order;
		typedef agg::renderer_base<pixfmt> renderer_base;
		typedef agg::renderer_scanline_aa_solid<renderer_base> renderer_aa;

	public:
		agg_drawer()
			: _balls(c_balls)
		{ _balls.resize(c_balls_number);	}

	private:
		virtual void draw(::bitmap &surface, Timings &timings)
		{
			LARGE_INTEGER counter;
			const float dt = 0.3f * (float)stopwatch(_balls_timer);

			stopwatch(counter);
				agge::fill(surface, solid_color_brush(aggx::rgba8(255, 255, 255)));
			timings.clearing += stopwatch(counter);

			stopwatch(counter);
				agg_path_adaptor p(_spiral);
				agg::conv_stroke<agg_path_adaptor> stroke(p);
				stroke.width(3);
				_spiral_flattened.clear();
				flatten<double>(_spiral_flattened, stroke);
			timings.stroking += stopwatch(counter);

			bitmap_rendering_buffer rbuf(surface);
			pixfmt pixf(rbuf);
			renderer_base rb(pixf);
			renderer_aa ren_aa(rb);

			if (_balls.empty())
			{
				stopwatch(counter);
				_rasterizer.add_path(agg_path_adaptor(_spiral_flattened));
				_rasterizer.sort();
				timings.rasterization += stopwatch(counter);
				ren_aa.color(agg::rgba8(0, 154, 255, 255));
				agg::render_scanlines(_rasterizer, _scanline, ren_aa);
				timings.rendition += stopwatch(counter);
			}

			for_each(_balls.begin(), _balls.end(), [&] (ball &b) {
				demo::move_and_bounce(b, dt, surface.width(), surface.height());
			});

			for_each(_balls.begin(), _balls.end(), [&] (ball &b) {
				agg::ellipse e(b.x, b.y, b.radius, b.radius);

				_rasterizer.reset();

				stopwatch(counter);
				_rasterizer.add_path(e);
				_rasterizer.sort();
				timings.rasterization += stopwatch(counter);
				ren_aa.color(agg::rgba8(b.color.r, b.color.g, b.color.b, b.color.a));
				agg::render_scanlines(_rasterizer, _scanline, ren_aa);
				timings.rendition += stopwatch(counter);
			});
		}

		virtual void resize(int width, int height)
		{
			_spiral.clear();
			spiral(_spiral, width / 2, height / 2, 5, (std::min)(width, height) / 2 - 10, 1, 0);
		}

	private:
		agg::rasterizer_scanline_aa<agg::rasterizer_sl_no_clip> _rasterizer;
		agg::scanline_u8 _scanline;
		AggPath _spiral, _spiral_flattened;
		LARGE_INTEGER _balls_timer;
		vector<demo::ball> _balls;
	};


	class agge_drawer : public Drawer
	{
	public:
		typedef blender<blender_used> solid_color_brush;

	public:
		agge_drawer()
			: _renderer(c_thread_count), _balls(c_balls)
		{ _balls.resize(c_balls_number);	}

	private:
		virtual void draw(::bitmap &surface, Timings &timings)
		{
			LARGE_INTEGER counter;
			const float dt = 0.3f * (float)stopwatch(_balls_timer);

			stopwatch(counter);
				agge::fill(surface, solid_color_brush(aggx::rgba8(255, 255, 255)));
			timings.clearing += stopwatch(counter);

			if (_balls.empty())
			{
				stopwatch(counter);
					agg_path_adaptor p(_spiral);
					aggx::conv_stroke<agg_path_adaptor> stroke(p, _vertex_storage, _coord_storage);
					stroke.width(3);
					_spiral_flattened.clear();
					flatten<aggx::real>(_spiral_flattened, stroke);
				timings.stroking += stopwatch(counter);

				solid_color_brush brush(aggx::rgba8(0, 154, 255, 230));
				solid_color_brush brush2(aggx::rgba8(0, 0, 0, 160));
				solid_color_brush brush3(aggx::rgba8(0, 0, 0, 255));

				stopwatch(counter);
				_rasterizer.add_path(agg_path_adaptor(_spiral_flattened));
				_rasterizer.prepare();
				timings.rasterization += stopwatch(counter);
				_renderer(surface, 0, _rasterizer.get_mask(), brush, aggx::calculate_alpha<8>());
				timings.rendition += stopwatch(counter);


				AggPath::value_type angle[] = {
					make_pair(make_pair(140.0f, 65.0f), aggx::path_cmd_move_to),
					make_pair(make_pair(180.0f, 70.0f), aggx::path_cmd_line_to),
					make_pair(make_pair(160.0f, 75.0f), aggx::path_cmd_line_to),
				};

				AggPath anglePath(angle, angle + _countof(angle));
				agg_path_adaptor p2(anglePath);

				aggx::conv_stroke<agg_path_adaptor> stroke2(p2, _vertex_storage, _coord_storage);

				stroke2.width(31);
				stroke2.line_join(aggx::miter_join);
				stroke2.line_cap(aggx::butt_cap);
				stroke2.miter_limit(10);

				_rasterizer.add_path(stroke2);
				_rasterizer.prepare();
				timings.rasterization += stopwatch(counter);
				_renderer(surface, 0, _rasterizer.get_mask(), brush2, aggx::calculate_alpha<8>());

				aggx::conv_stroke<agg_path_adaptor> stroke3(p2, _vertex_storage, _coord_storage);

				stroke3.width(1);
				stroke3.line_join(aggx::miter_join);
				stroke3.line_cap(aggx::butt_cap);
				stroke3.miter_limit(10);

				_rasterizer.add_path(stroke3);
				_rasterizer.prepare();
				timings.rasterization += stopwatch(counter);
				_renderer(surface, 0, _rasterizer.get_mask(), brush3, aggx::calculate_alpha<8>());
			}

			for_each(_balls.begin(), _balls.end(), [&] (ball &b) {
				demo::move_and_bounce(b, dt, surface.width(), surface.height());
			});

			for_each(_balls.begin(), _balls.end(), [&] (ball &b) {
				aggx::ellipse e(b.x, b.y, b.radius, b.radius);

				_rasterizer.reset();

				stopwatch(counter);
				_rasterizer.add_path(e);
				_rasterizer.prepare();
				timings.rasterization += stopwatch(counter);
				_renderer(surface, 0, _rasterizer.get_mask(), agge_drawer::solid_color_brush(b.color), aggx::calculate_alpha<8>());
				timings.rendition += stopwatch(counter);
			});
		}

		virtual void resize(int width, int height)
		{
			_spiral.clear();
			spiral(_spiral, width / 2, height / 2, 5, (std::min)(width, height) / 2 - 10, 1, 0);
		}

	private:
		aggx::rasterizer_scanline_aa<agg::rasterizer_sl_no_clip> _rasterizer;
		agge::renderer_parallel _renderer;
		aggx::vertex_sequence<aggx::vertex_dist> _vertex_storage;
		vector<aggx::point_r> _coord_storage;
		AggPath _spiral, _spiral_flattened;
		LARGE_INTEGER _balls_timer;
		vector<demo::ball> _balls;
	};
}


int main()
{
	delete new int;

	agg_drawer d1;
	agge_drawer d2;

	MainDialog dlg(c_use_original_agg ? (Drawer &)d1 : (Drawer &)d2);

	MainDialog::PumpMessages();
}
