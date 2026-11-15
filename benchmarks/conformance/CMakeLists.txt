function(add_cbor_conformance library header link)
    set(adapters "")
    foreach(adapter ${ARGN})
        string(APPEND adapters "X(${adapter}) ")
    endforeach()
    set(target conformance_${library})
    add_executable(${target} main.cpp)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/benchmarks)
    target_compile_definitions(${target} PRIVATE
        DATA_DIR="${CMAKE_SOURCE_DIR}/data"
        ADAPTER_HEADER="${header}"
        "CONFORMANCE_ADAPTERS=${adapters}"
    )
    target_link_libraries(${target} PRIVATE ${link})
    target_compile_options(${target} PRIVATE -Wall -Wextra)
endfunction()

add_cbor_conformance(join adapters/join.hpp join::cbor JoinAdapter JoinZeroCopyAdapter)
add_cbor_conformance(jsoncons adapters/jsoncons.hpp jsoncons::jsoncons JsonConsAdapter)
add_cbor_conformance(libcbor adapters/libcbor.hpp libcbor::libcbor LibCborAdapter)
add_cbor_conformance(nlohmann adapters/nlohmann.hpp nlohmann_json::nlohmann_json NlohmannAdapter)
add_cbor_conformance(taocpp adapters/taocpp.hpp taocpp::json TaoCppAdapter)
add_cbor_conformance(tinycbor adapters/tinycbor.hpp tinycbor TinyCborAdapter)
