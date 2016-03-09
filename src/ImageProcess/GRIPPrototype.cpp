#include <iostream>
#include <string>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << " Usage: display_image ImageToLoadAndDisplay" << std::endl;
        return -1;
    }

    cv::Mat img;
    img = cv::imread(argv[1], CV_LOAD_IMAGE_COLOR);   // Read the file

    if (!img.data) {                            // Check for invalid input
        std::cout <<  "Could not open or find the image" << std::endl;
        return -1;
    }

    cv::Mat rgb;
    cv::inRange(img, cv::Scalar(0, 150, 0), cv::Scalar(255, 255, 255), rgb);


    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(rgb, contours, hierarchy, cv::RETR_TREE,
                     cv::CHAIN_APPROX_SIMPLE);

    std::vector<std::vector<cv::Point>> filtered;
    for (auto& contour : contours) {
        if (cv::arcLength(contour,
                          true) > 100 && cv::arcLength(contour, true) < 350) {
            filtered.emplace_back(contour);
        }
    }
    std::cout << filtered.size() << std::endl;

    cv::drawContours(rgb, filtered, -1, cv::Scalar(255, 255, 255), 2);


// now find center?
    findContours(rgb,
                 filtered,
                 hierarchy,
                 cv::CV_RETR_TREE,
                 cv::CV_CHAIN_APPROX_SIMPLE);

    std::vector<cv::Moments> mu(filtered.size());
    for (int i = 0; i < filtered.size(); i++) {
        mu[i] = cv::moments(filtered[i], false);
    }

    std::vector<cv::Point2f> mc(filtered.size());
    for (int i = 0; i < filtered.size(); i++) {
        mu[i] = cv::Point2f(mu[i].m10 / mu[i].m00, mu[i].m01 / mu[i].m00);
    }

    cv::imshow("contours", rgb);
    cv::waitKey();
}
