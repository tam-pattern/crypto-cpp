#include "starkware/crypto/ecdsa.h"

#include "starkware/algebra/fraction_field_element.h"
#include "starkware/crypto/elliptic_curve_constants.h"
#include "starkware/utils/error_handling.h"
#include "starkware/utils/prng.h"

namespace starkware {

EcPoint<PrimeFieldElement> GetPublicKey(const PrimeFieldElement::ValueType& private_key) {
  const auto& generator = GetEcConstants().k_points[1];
  const auto& alpha = GetEcConstants().k_alpha;
  return generator.MultiplyByScalar(private_key, alpha);
}

Signature SignEcdsa(
    const PrimeFieldElement::ValueType& private_key, const PrimeFieldElement& z,
    const PrimeFieldElement::ValueType& k) {
  using ValueType = typename PrimeFieldElement::ValueType;
  const auto& generator = GetEcConstants().k_points[1];
  const auto& alpha = GetEcConstants().k_alpha;
  const auto& curve_order = GetEcConstants().k_order;
  constexpr auto upper_bound = 0x800000000000000000000000000000000000000000000000000000000000000_Z;
  static_assert(upper_bound <= PrimeFieldElement::kModulus);
  ASSERT_PATTER(upper_bound <= curve_order, "Unexpected curve size.");

  ASSERT_PATTER(z != PrimeFieldElement::Zero(), "Message cannot be zero.");
  ASSERT_PATTER(z.ToStandardForm() < upper_bound, "z is too big.");
  ASSERT_PATTER(k != ValueType::Zero(), "k must not be zero");

  const PrimeFieldElement x = generator.MultiplyByScalar(k, alpha).x;
  const ValueType r = x.ToStandardForm();
  ASSERT_PATTER(
      (r < curve_order) && (r != ValueType::Zero()),
      "Bad randomness, please try a different a different k.");

  const ValueType k_inv = k.InvModPrime(curve_order);
  ValueType s = ValueType::MulMod(r, private_key, curve_order);
  // Non modular addition, requires the summands to be small enough to prevent overflow.
  ASSERT_PATTER(curve_order.NumLeadingZeros() > 0, "Implementation assumes smaller curve.");
  s = s + z.ToStandardForm();
  s = ValueType::MulMod(s, k_inv, curve_order);
  ASSERT_PATTER(s != ValueType::Zero(), "Bad randomness, please try a different k.");

  const ValueType w = s.InvModPrime(curve_order);
  ASSERT_PATTER(w < upper_bound, "Bad randomness, please try a different k.");
  const PrimeFieldElement w_field = PrimeFieldElement::FromBigInt(w);
  return {x, w_field};
}

bool VerifyEcdsa(
    const EcPoint<PrimeFieldElement>& public_key, const PrimeFieldElement& z,
    const Signature& sig) {
  using FractionFieldElementT = FractionFieldElement<PrimeFieldElement>;
  using EcPointT = EcPoint<FractionFieldElementT>;
  const auto& r = sig.first;
  const auto& w = sig.second;
  // z, r, w should be smaller than 2^251.
  const auto upper_bound = 0x800000000000000000000000000000000000000000000000000000000000000_Z;
  ASSERT_PATTER(z != PrimeFieldElement::Zero(), "Message cannot be zero.");
  ASSERT_PATTER(z.ToStandardForm() < upper_bound, "z is too big.");
  ASSERT_PATTER(r != PrimeFieldElement::Zero(), "r cannot be zero.");
  ASSERT_PATTER(r.ToStandardForm() < upper_bound, "r is too big.");
  ASSERT_PATTER(w != PrimeFieldElement::Zero(), "w cannot be zero.");
  ASSERT_PATTER(w.ToStandardForm() < upper_bound, "w is too big.");
  const FractionFieldElementT alpha(GetEcConstants().k_alpha);
  const auto generator = GetEcConstants().k_points[1];
  const auto zw = PrimeFieldElement::ValueType::MulMod(
      z.ToStandardForm(), w.ToStandardForm(), GetEcConstants().k_order);
  const EcPointT zw_g = generator.ConvertTo<FractionFieldElementT>().MultiplyByScalar(zw, alpha);
  const auto rw = PrimeFieldElement::ValueType::MulMod(
      r.ToStandardForm(), w.ToStandardForm(), GetEcConstants().k_order);
  const EcPointT rw_q = public_key.ConvertTo<FractionFieldElementT>().MultiplyByScalar(rw, alpha);
  return (zw_g + rw_q).x.ToBaseFieldElement() == r || (zw_g - rw_q).x.ToBaseFieldElement() == r;
}

bool VerifyEcdsaPartialKey(
    const PrimeFieldElement& public_key_x, const PrimeFieldElement& z, const Signature& sig) {
  const auto alpha = GetEcConstants().k_alpha;
  const auto beta = GetEcConstants().k_beta;
  const auto public_key = EcPoint<PrimeFieldElement>::GetPointFromX(public_key_x, alpha, beta);
  ASSERT_PATTER(
      public_key.has_value(), "Given public key (" + public_key_x.ToString() +
                                  ") does not correspond to a valid point on the elliptic curve.");

  // There are two points on the elliptic curve with the given public_key_x, both will be
  // tested by VerifyEcdsa().
  return VerifyEcdsa(*public_key, z, sig);
}

}  // namespace starkware
