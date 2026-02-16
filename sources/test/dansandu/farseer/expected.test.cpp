#include "dansandu/farseer/expected.hpp"
#include "dansandu/radiance/radiance.hpp"

using dansandu::farseer::expected::Expected;

TEST_CASE("expected")
{
    SECTION("success")
    {
        const auto expectedValue = 17;

        const auto expected = Expected<int>::fromSuccess(expectedValue);

        REQUIRE(expected.success());

        REQUIRE(!expected.failure());

        REQUIRE(expected.getValue() == expectedValue);

        REQUIRE_THROW(std::bad_variant_access, expected.getErrorCode());

        REQUIRE_THROW(std::bad_variant_access, expected.getErrorMessage());
    }

    SECTION("failure")
    {
        const auto errorCode = static_cast<uint32_t>(13);

        const auto errorMessage = "My error message";

        const auto expected = Expected<int>::fromFailure(errorCode, errorMessage);

        REQUIRE(!expected.success());

        REQUIRE(expected.failure());

        REQUIRE(expected.getErrorCode() == errorCode);

        REQUIRE(expected.getErrorMessage() == errorMessage);

        REQUIRE_THROW(std::bad_variant_access, expected.getValue());
    }
}
