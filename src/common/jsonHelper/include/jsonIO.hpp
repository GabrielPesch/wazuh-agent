#pragma once

#include <filesystem>
#include <fstream>

namespace Utils
{
    /// @brief Class to read and write json files
    template<typename T>
    class JsonIO
    {
    public:
        /// @brief Read a json file
        /// @param filePath Path to the json file
        /// @return Json object
        static T readJson(const std::filesystem::path& filePath)
        {
            std::ifstream file(filePath);

            if (!file.is_open())
            {
                throw std::runtime_error("Could not open file");
            }

            T json;
            file >> json;
            return json;
        }

        /// @brief Write a json file
        /// @param filePath Path to the json file
        /// @param json Json object
        static void writeJson(const std::filesystem::path& filePath, const T& json)
        {
            std::ofstream file(filePath);

            if (!file.is_open())
            {
                throw std::runtime_error("Could not open file");
            }

            file << json;

            if (!file.good())
            {
                throw std::runtime_error("Could not write file");
            }
        }
    };
} // namespace Utils
