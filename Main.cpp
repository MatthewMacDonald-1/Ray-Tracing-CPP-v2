#include "rtweekend.h"

#include "color.h"
#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>



color ray_color(const ray& r, const hittable& world, int depth) {
	hit_record rec;

	// If we've exceed the ray bounce limit, no more light is gathered
	if (depth <= 0) {
		return color(0,0,0);
	}

	if (world.hit(r, 0.001, infinity, rec)) {
		ray scattered;
		color attenuation;
		if (rec.mat_ptr->scatter(r, rec, attenuation, scattered))
			return attenuation * ray_color(scattered, world, depth-1);
		return color(0,0,0);
	}
	vec3 unit_direction = unit_vector(r.direction());
	auto t = 0.5 * (unit_direction.y() + 1.0);
	return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

/**
 * Scene with 3 spheres and ground sphere. 
 * Left: Hollow glass
 * Center: blue Lambertain
 * Right: Bronze metal
*/
hittable_list scene_a() {
	hittable_list world;

	auto material_ground = make_shared<lambertian>(color(0.8, 0.8, 0.0));
	auto material_center = make_shared<lambertian>(color(0.1, 0.2, 0.5));
	auto material_left = make_shared<dielectric>(1.5);
	auto material_right = make_shared<metal>(color(0.8, 0.6, 0.2), 0.0);

	world.add(make_shared<sphere>(point3(0.0, -100.5, -1.0), 100.0, material_ground));
	world.add(make_shared<sphere>(point3(0.0, 0.0, -1.0), 0.5, material_center));
	world.add(make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.5, material_left));
	world.add(make_shared<sphere>(point3(-1.0, 0.0, -1.0), -0.45, material_left));
	world.add(make_shared<sphere>(point3(1.0, 0.0, -1.0), 0.5, material_right));

	return world;
}

/**
 * A random scene with lots of small spheres and three distinct large spheres
*/
hittable_list random_scene() {
	hittable_list world;
	
	auto material_ground = make_shared<lambertian>(color(0.5, 0.5, 0.5));
	world.add(make_shared<sphere>(point3(0,-1000, 0), 1000, material_ground));

	// Random spheres
	for (int a = -11; a < 11; a++) {
		for (int b = -11; b < 11; b++) {
			auto choose_mat = random_double();
			point3 center(a + 0.9*random_double(), 0.2, b + 0.9*random_double());

			if ((center - point3(4, 0.2, 0)).length() > -0.9) {
				shared_ptr<material> sphere_material;

				if (choose_mat < 0.8) {
					// Diffuse
					auto albedo = color::random() * color::random();
					sphere_material = make_shared<lambertian>(albedo);
					world.add(make_shared<sphere>(center, 0.2, sphere_material));
				} else if (choose_mat < 0.95) {
					// metal 
					auto albedo = color::random(0.5, 1);
					auto fuzz = random_double(0,0.5);
					sphere_material = make_shared<metal>(albedo, fuzz);
					world.add(make_shared<sphere>(center, 0.2, sphere_material));
				} else {
					sphere_material = make_shared<dielectric>(1.5);
					world.add(make_shared<sphere>(center, 0.2, sphere_material));
				}
			}
		}
	}


	// 3 distinct spheres

	auto mat_1 = make_shared<dielectric>(1.5);
	world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, mat_1));

	auto mat_2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
	world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, mat_2));

	auto mat_3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
	world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, mat_3));


	return world;
}

/**
 * Single core render
*/
void render_world_sc(hittable_list& world, camera cam, int image_width, int image_height, int samples_per_pixel, int max_depth) {
	std::cout << "P3\n" << image_width << " " << image_height << "\n255\n";

	int max = image_height * image_width;
	int count = 0;

	for (int j = image_height - 1; j >= 0; --j) {
		// std::cerr << "\rScan lines remaining: " << j << ' ' << std::flush;
		for (int i = 0; i < image_width; ++i) {
			std::cerr << "\rPixels complete: " << (max - count) << " / " << max << ' ' << std::flush;
			color pixel_color(0, 0, 0);
			for (int s = 0; s < samples_per_pixel; ++s) {
				auto u = (i + random_double()) / (image_width - 1);
				auto v = (j + random_double()) / (image_height - 1);
				ray r = cam.get_ray(u, v);
				pixel_color += ray_color(r, world, max_depth);
			}
			write_color(std::cout, pixel_color, samples_per_pixel);
			count++;
		}
	}

	std::cerr << "\nDone\n"; // cerr writes to the error output stream
}

/**
 * 
*/
unsigned char* colors_to_byte_array(color* pixels, int length, int samples_per_pixel) {
	unsigned char* byte_array = (unsigned char*)malloc(3 * length);

	for (int i = 0; i < length; i++) {
		int byte_index = 3 * i;
		pixels[i] = correct_color_and_gamma(pixels[i], samples_per_pixel);
		byte_array[byte_index] = static_cast<unsigned char>(256 * clamp(pixels[i].x(), 0.0, 0.999));
		byte_array[byte_index + 1] = static_cast<unsigned char>(256 * clamp(pixels[i].y(), 0.0, 0.999));
		byte_array[byte_index + 2] = static_cast<unsigned char>(256 * clamp(pixels[i].z(), 0.0, 0.999));
	}

	return byte_array;
}

/**
 * Multi core render
*/
color* render_world_mt(hittable_list& world, camera cam, int image_width, int image_height, int samples_per_pixel, int max_depth) {
	int cores = std::thread::hardware_concurrency();
	volatile std::atomic<std::size_t> count(0);
	std::vector<std::future<void>> future_vector;

	std::cerr << "Core count: " << cores + '\n';

	int max = image_width * image_height;
	color *rawPixelColors = (color*)malloc(sizeof(color) * max);

	while (cores--) 
		future_vector.emplace_back(
			std::async([=, &world, &count, &rawPixelColors, &image_width, &image_height]()
			{
				while (true)
				{
					int index = count++;
					if (index > max)
						break;
					int x = index % image_width;
					int y = index / image_width;
					color pixel_color(0, 0, 0);
					for (int s = 0; s < samples_per_pixel; ++s) {
						auto u = (x + random_double()) / (image_width - 1);
						auto v = (y + random_double()) / (image_height - 1);
						ray r = cam.get_ray(u, v);
						pixel_color += ray_color(r, world, max_depth);
					}
					rawPixelColors[y * image_width + x] = pixel_color;
				}
			}
		)
	);
	using namespace std::chrono;
	using namespace std::this_thread;

	while (!(count >= max)) {
		// Wait
		sleep_for(nanoseconds(1));
    	sleep_until(system_clock::now() + seconds(1));
		std::cerr << "\rPixels complete: " << (max - count) << " / " << max << ' ' << std::flush;
	}

	// std::cout << "P3\n" << image_width << " " << image_height << "\n255\n";
	// for (int j = image_height - 1; j >= 0; --j) {
	// 	// std::cerr << "\rScan lines remaining: " << j << ' ' << std::flush;
	// 	for (int i = 0; i < image_width; ++i) {
	// 		write_color(std::cout, rawPixelColors[j * image_width + i], samples_per_pixel);
	// 	}
	// }

	//free(rawPixelColors); // free memory

	std::cerr << "\nDone\n"; // cerr writes to the error output stream

	return rawPixelColors;
}

void render_world_mt_ppm(hittable_list& world, camera cam, int image_width, int image_height, int samples_per_pixel, int max_depth) {
	color* pixels = render_world_mt(world, cam, image_width, image_height, samples_per_pixel, max_depth);

	std::cout << "P3\n" << image_width << " " << image_height << "\n255\n";
	for (int j = image_height - 1; j >= 0; --j) {
		for (int i = 0; i < image_width; ++i) {
			write_color(std::cout, pixels[j * image_width + i], samples_per_pixel);
		}
	}

	free(pixels); // free memory
}

/**
 * Returns a random alphanumeric character
*/
char genRandomChar() {
	int select = (int)random_double(1.0, 3.0);
	char randChar;
	switch (select)
	{
	case 1:
		randChar = (char)random_double(48, 57);
		break;
	case 2:
		randChar = (char)random_double(97, 122);
		break;
	case 3:
		randChar = (char)random_double(65, 90);
		break;
	
	default:
		randChar = (char)random_double(97, 122);
		break;
	}

	return randChar;
}

int main() {
	// Image
	// const auto aspect_ratio = 4.0 / 3.0;
	const auto aspect_ratio = 16.0 / 9.0;
	const int image_width = 1024;
	const int image_height = static_cast<int>(image_width / aspect_ratio);
	const int samples_per_pixel = 10;
	const int max_depth = 10;

	// World
	auto R = cos(pi/4);
	hittable_list world = random_scene();

	std::string rand;

	for (int i = 0; i < 16; i++) {
		if (rand.length() > 0) 
			rand += genRandomChar();
		else
			rand = genRandomChar();
	}

	std::cerr << "Name: " << rand << std::endl;


	// Camera
	point3 lookfrom(13,2,3);
	// point3 lookfrom(0,1,-10);
	point3 lookat(0,0,0);
	vec3 vup(0,1,0);
	auto dist_to_focus = 10.0;
	auto aperture = 0.1;
	camera cam(lookfrom, lookat, vup, 20.0, aspect_ratio, aperture, dist_to_focus);

	// Render

	// render_world_sc(world, cam, image_width, image_height, samples_per_pixel, max_depth);
	color* pixels = render_world_mt(world, cam, image_width, image_height, samples_per_pixel, max_depth);

	unsigned char* byte_array = colors_to_byte_array(pixels, image_width * image_height, samples_per_pixel);
	free(pixels); // free memory
	stbi_flip_vertically_on_write(true);
	std::string fileName = "renders/";
	fileName.append(rand);
	fileName.append(".bmp");
	int result = stbi_write_bmp(fileName, image_width, image_height, 3, byte_array); // TODO: flip image vertically when saving
	free(byte_array); // free memory

	std::cerr << "Result: " << result << ' ' << std::flush;

	return 0;
}