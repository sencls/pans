#include<pans/macros.h>
#include<iostream>
#include<limits>
#include<type_traits>

static_assert(sizeof(u8)==1 && std::is_unsigned_v<u8>);
static_assert(sizeof(s8)==1 && std::is_signed_v<s8>);
static_assert(sizeof(u16)==2 && std::is_unsigned_v<u16>);
static_assert(sizeof(s16)==2 && std::is_signed_v<s16>);
static_assert(sizeof(u32)==4 && std::is_unsigned_v<u32>);
static_assert(sizeof(s32)==4 && std::is_signed_v<s32>);
static_assert(sizeof(u64)==8 && std::is_unsigned_v<u64>);
static_assert(sizeof(s64)==8 && std::is_signed_v<s64>);
static_assert(INVALID8 ==MAX_U8);
static_assert(INVALID16 ==MAX_U16);
static_assert(INVALID32 ==MAX_U32);
static_assert(INVALID64 ==MAX_U64);
static_assert(MAX_U8 ==  std::numeric_limits<u8>::max());
static_assert(MAX_U16 == std::numeric_limits<u16>::max());
static_assert(MAX_U32 == std::numeric_limits<u32>::max());
static_assert(MAX_U64 == std::numeric_limits<u64>::max());

void test_types_and_constants()
{
    //运行时检查，if语句判断混合assert的包装
    ASSERT_NOEFFECT(sizeof(u8)==1 && std::is_unsigned_v<u8>);
    ASSERT_NOEFFECT(sizeof(s8)==1 && std::is_signed_v<s8>);
    ASSERT_NOEFFECT(sizeof(u16)==2 && std::is_unsigned_v<u16>);
    ASSERT_NOEFFECT(sizeof(s16)==2 && std::is_signed_v<s16>);
    ASSERT_NOEFFECT(sizeof(u32)==4 && std::is_unsigned_v<u32>);
    ASSERT_NOEFFECT(sizeof(s32)==4 && std::is_signed_v<s32>);
    ASSERT_NOEFFECT(sizeof(u64)==8 && std::is_unsigned_v<u64>);
    ASSERT_NOEFFECT(sizeof(s64)==8 && std::is_signed_v<s64>);
    ASSERT_NOEFFECT(INVALID8 ==MAX_U8);
    ASSERT_NOEFFECT(INVALID16 ==MAX_U16);
    ASSERT_NOEFFECT(INVALID32 ==MAX_U32);
    ASSERT_NOEFFECT(INVALID64 ==MAX_U64);
    ASSERT_RETNONE(MAX_U8 ==  std::numeric_limits<u8>::max());
    ASSERT_RETNONE(MAX_U16 == std::numeric_limits<u16>::max());
    ASSERT_RETNONE2(MAX_U32 == std::numeric_limits<u32>::max(), "this is a test for ASSERT_RETNONE2 macro");
    ASSERT_RETNONE2(MAX_U64 == std::numeric_limits<u64>::max(), "this is a test for ASSERT_RETNONE2 macro");
}

[[nodiscard]] int test_assert_retval()
{
    ASSERT_RETVAL(false,-1);
    return 0;
}

[[nodiscard]] int test_assert_retva2()
{
    ASSERT_RETVAL2(false,-2,"this should not trigger an assertion.");
    return 0;
}

void test_assert_macros()
{
    ASSERT_NOEFFECT2(true, "this should not trigger an assertion");
    int never_reacher_condition =5;
    int i=0;

    for(i=0;i<10;++i)
    {
        ASSERT_CONTINUE(i !=never_reacher_condition);
        std::cout <<"AAAA------- "<<i <<std::endl;
    }
    std::cout <<"----------------------我是分界线----------当前i = ["<<i<<"]--------------------------\n";
    for(i=0;i<10;++i)
    {
        ASSERT_CONTINUE2(i !=never_reacher_condition,"loop index should not be" <<i);
    }
    std::cout <<"----------------------我是分界线-----------当前i = ["<<i<<"]-----------------------------\n";
    for(i=0;i<10;++i)
    {
        ASSERT_BREAK(i !=never_reacher_condition);
        std::cout <<"BBBB------- "<<i <<std::endl;
    }
    std::cout <<"----------------------我是分界线----------当前i = ["<<i<<"]------------------------------\n";
    for(i=0;i<10;++i)
    {
        ASSERT_BREAK2(i !=never_reacher_condition,"loop index should not be" <<i);
    }
    std::cout <<"----------------------我是分界线----------当前i = ["<<i<<"]------------------------------\n";
    for(i=0;i<10;++i)
    {
        ASSERT_NOEFFECT(i !=never_reacher_condition);
        std::cout <<"CCCC------- "<<i <<std::endl;
    }
    std::cout <<"----------------------我是分界线----------当前i = ["<<i<<"]------------------------------\n";
    for(i=0;i<10;++i)
    {
        ASSERT_NOEFFECT2(i !=never_reacher_condition,"loop index should not be" <<i);
    }
    std::cout <<"----------------------我是分界线----------当前i = ["<<i<<"]------------------------------\n";
}

int main() {
#ifdef PANS_DEBUG
    std::cout <<"This program is compiled in debug mode. Assert will terminate the program on failure"<<std::endl;
#endif

#ifdef NDEBUG
    std::cout <<"This program is compiled in release mode. Assert will do nothing on failure"<<std::endl;
#endif
  
    test_types_and_constants();
    test_assert_macros();

    auto retval1 =test_assert_retval();
    std::cout <<"test_assert_retval() returnd: "<<retval1 <<std::endl;
    auto retval2 =test_assert_retva2();
    std::cout <<"test_assert_retva2() returnd: "<<retval2 <<std::endl;

    return EXIT_SUCCESS;
}

