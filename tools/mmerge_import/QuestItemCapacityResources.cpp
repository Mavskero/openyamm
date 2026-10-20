#include "QuestItemCapacityResources.h"
#include "QuestItemCapacitySource.h"

#include <CommonCrypto/CommonDigest.h>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace OpenYAMM::Import
{
std::string buildQuestItemCapacityOverlay(const std::vector<uint8_t> &globalEventBytes, bool chinese)
{
    std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> hash;
    CC_SHA256(globalEventBytes.data(), static_cast<CC_LONG>(globalEventBytes.size()), hash.data());
    std::ostringstream digest;
    for (unsigned char value : hash)
    {
        digest << std::hex << std::setfill('0') << std::setw(2) << unsigned(value);
    }
    if (digest.str() != "48dccfb27066fd84bc2c7849a462defb97fd34944251b1274e1c24595a8a7138")
    {
        throw std::runtime_error("Unsupported Global.evt fingerprint for quest item capacity guards");
    }
    const std::string message = chinese
        ? "请先整理物品，再来领取奖励。"
        : "Please make room for your items, then ask for the reward again.";
    return "local capacityMessage = \"" + message + "\"\n" + std::string(QuestItemCapacitySource);
}
}
