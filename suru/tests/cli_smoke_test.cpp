#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(CliSmokeTest, HelpCommandSucceeds) {
    const std::vector<std::string> args = ::testing::internal::GetArgvs();
    ASSERT_FALSE(args.empty());
    const std::filesystem::path self = std::filesystem::path(args.at(0)).parent_path();

#if defined(_WIN32)
    const std::filesystem::path candidate = self / "suru.exe";
#else
    const std::filesystem::path candidate = self / "suru";
#endif

    ASSERT_TRUE(std::filesystem::exists(candidate)) << "missing suru executable next to cli_smoke_test";
    const std::string suru_path = candidate.string();

    const std::string cmd = "\"" + suru_path + "\" --help";

    const int rc = std::system(cmd.c_str());
    EXPECT_EQ(rc, 0);
}

