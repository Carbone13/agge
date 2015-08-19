#pragma once

#include <agge/tools.h>
#include <agge/types.h>

#include <utee/ut/assert.h>
#include <utility>
#include <vector>

namespace agge
{
	namespace tests
	{
		namespace mocks
		{
			struct cell
			{
				int x;
				int area;
				int cover;
			};

			class path
			{
			public:
				struct point
				{
					real_t x, y;
					int command;
				};

			public:
				path()
					: position(0)
				{	}

				template <typename T, size_t n>
				path(T (&points_)[n])
					: points(points_, points_ + n), position(0)
				{	}

				void rewind(unsigned /*path_id*/)
				{	position = 0;	}

				int vertex(real_t *x, real_t *y)
				{
					if (position < points.size())
					{
						*x = points[position].x, *y = points[position].y;
						return points[position].command;
					}
					return path_command_stop;
				}

			public:
				std::vector<point> points;
				size_t position;
			};

			template <size_t precision>
			class mask
			{
			public:
				typedef std::pair<const cell * /*begin*/, const cell * /*end*/> scanline_cells;

				enum { _1_shift = precision };

			public:
				template <typename T, int n>
				mask(const T (&cells)[n], int y0)
					: _min_y(y0), _height(n)
				{
					int min_x = 0x7FFFFFFF, max_x = -0x7FFFFFFF;

					for (int i = 0; i != n; ++i)
					{
						_cells.push_back(cells[i]);
						for (int j = 0; j != cells[i].second - cells[i].second; ++j)
						{
							min_x = agge_min(min_x, (cells[i].first + j)->x);
							max_x = agge_max(min_x, (cells[i].first + j)->x);
						}
					}
					_width = agge_max(max_x - min_x, -1) + 1;
				}

				scanline_cells operator [](int y) const
				{	return _cells.at(y - _min_y);	}

				int min_y() const
				{	return _min_y;	}

				int height() const
				{	return _height;	}

			protected:
				int width() const
				{	return _width;	}

			private:
				int _width, _min_y, _height;
				std::vector<scanline_cells> _cells;
			};


			template <size_t precision>
			class mask_full : public mask<precision>
			{
			public:
				template <typename T, int n>
				mask_full(const T (&cells)[n], int y0)
					: mask<precision>(cells, y0)
				{	}

				using mask<precision>::width;
			};


			template <typename CoverT = uint8_t>
			class renderer_adapter
			{
			public:
				typedef CoverT cover_type;

				struct render_log_entry
				{
					int x;
					std::vector<cover_type> covers;

					bool operator ==(const render_log_entry &rhs) const
					{	return x == rhs.x && covers == rhs.covers;	}
				};

			public:
				bool set_y(int y)
				{
					current_y = y;
					return set_y_result;
				}

				void operator ()(int x, count_t length, const cover_type *covers)
				{
					render_log_entry e = { x, vector<cover_type>(covers, covers + length) };
					render_log.push_back(e);
					raw_render_log.push_back(make_pair(covers, length));
				}

			public:
				int current_y;
				bool set_y_result;
				std::vector<render_log_entry> render_log;
				std::vector< std::pair<const cover_type *, count_t> > raw_render_log;
			};


			template <typename PixelT, size_t guard_size = 0>
			class bitmap
			{
			public:
				typedef PixelT pixel;

			public:
				bitmap(count_t width, count_t height)
					: _width(width), _height(height), data((width + guard_size) * height)
				{	}

				pixel *row_ptr(count_t y)
				{	return &data[y * (_width + guard_size)];	}

				count_t width() const
				{	return _width;	}

				count_t height() const
				{	return _height;	}

			public:
				std::vector<pixel> data;

			private:
				count_t _width, _height;
			};


			template <typename PixelT, typename CoverT>
			class blender
			{
			public:
				typedef PixelT pixel;
				typedef CoverT cover_type;

				struct fill_log_entry;

			public:
				void operator ()(PixelT *pixels, int x, int y, unsigned int length) const
				{
					fill_log_entry entry = { pixels, x, y, length };

					filling_log.push_back(entry);
				}

				void operator ()(PixelT *pixels, int x, int y, unsigned int length, const cover_type *covers) const
				{
					assert_not_equal(0u, length);

					int offset = sizeof(cover_type) * 8;
					int mask_x = 0x000000FF << offset;
					int mask_y = 0x0000FF00 << offset;

					for (; length; --length, ++pixels, ++covers)
						*pixels = static_cast<pixel>(static_cast<int>(*covers)
							+ ((x << offset) & mask_x)
							+ ((y << (offset + 8)) & mask_y));
				}

				mutable std::vector<fill_log_entry> filling_log;
			};


			template <typename PixelT, typename CoverT>
			struct blender<PixelT, CoverT>::fill_log_entry
			{
				pixel *pixels;
				unsigned int x;
				unsigned int y;
				unsigned int length;

				bool operator ==(const fill_log_entry &rhs) const
				{	return pixels == rhs.pixels && x == rhs.x && y == rhs.y && length == rhs.length;	}
			};


			template <typename T, size_t precision>
			struct simple_alpha
			{
				T operator ()(int area) const
				{	return static_cast<T>(area >> (precision + 1));	}
			};
		}
	}
}
