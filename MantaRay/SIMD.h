//
// Copyright (c) 2023 MantaRay authors. See the list of authors for more details.
// Licensed under MIT.
//

#ifndef MANTARAY_SIMD_H
#define MANTARAY_SIMD_H

#include <array>

#ifdef __AVX512BW__
#include "Backend/Avx512.h"
#elifdef __AVX2__
#include "Backend/Avx2.h"
#endif

namespace MantaRay
{

    /// \brief SIMD implementations for MantaRay.
    /// \details This class contains SIMD implementations of common operations used in MantaRay.
    class SIMD
    {

        public:
            /// \brief Add the delta to elements in the input arrays.
            /// \tparam T The type of the input and delta.
            /// \tparam InputSize The size of the input arrays.
            /// \tparam DeltaSize The size of the delta array.
            /// \param inputA The first input array.
            /// \param inputB The second input array.
            /// \param delta The delta array.
            /// \param oA The delta offset for the first input array.
            /// \param oB The delta offset for the second input array.
            /// \details This function adds the offset delta to elements in the input arrays.
            ///          The delta is added to the input arrays in-place.
            template<typename T, size_t InputSize, size_t DeltaSize>
            static inline void AddToAll(std::array<T, InputSize>& inputA, std::array<T, InputSize>& inputB,
                                        const std::array<T, DeltaSize>& delta,
                                        const uint32_t oA, const uint32_t oB)
            {
#ifdef __AVX512BW__
                // Define the registers used in the loops:
                Vec512I zmm0;
                Vec512I zmm1;

                // Define the step size for the loops:
                constexpr size_t Step = sizeof(Vec512I) / sizeof(T);

                //region INPUT A
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    zmm0 = Avx512<T>::From(inputA,      i);
                    zmm1 = Avx512<T>::From(delta , oA + i);

                    // Add the delta register to the input register:
                    zmm0 = Avx512<T>::Add(zmm0, zmm1);

                    // Store the result back from the input register to the input array:
                    Avx512<T>::Store(zmm0, inputA, i);
                }
                //endregion

                //region INPUT B
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    zmm0 = Avx512<T>::From(inputB,      i);
                    zmm1 = Avx512<T>::From(delta , oB + i);

                    // Add the delta register to the input register:
                    zmm0 = Avx512<T>::Add(zmm0, zmm1);

                    // Store the result back from the input register to the input array:
                    Avx512<T>::Store(zmm0, inputB, i);
                }
                //endregion
#elifdef __AVX2__
                // Define the registers used in the loops:
                Vec256I ymm0;
                Vec256I ymm1;

                // Define the step size for the loops:
                constexpr size_t Step = sizeof(Vec256I) / sizeof(T);

                //region INPUT A
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    ymm0 = Avx<T> ::From(inputA,      i);
                    ymm1 = Avx<T> ::From(delta , oA + i);

                    // Add the delta register to the input register:
                    ymm0 = Avx2<T>::Add(ymm0, ymm1);

                    // Store the result back from the input register to the input array:
                    Avx<T>::Store(ymm0, inputA, i);
                }
                //endregion

                //region INPUT B
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    ymm0 = Avx<T> ::From(inputB,      i);
                    ymm1 = Avx<T> ::From(delta , oB + i);

                    // Add the delta register to the input register:
                    ymm0 = Avx2<T>::Add(ymm0, ymm1);

                    // Store the result back from the input register to the input array:
                    Avx<T>::Store(ymm0, inputB, i);
                }
                //endregion
#else
                // Add the delta to the input arrays:
                for (size_t i = 0; i < InputSize; i++) inputA[i] += delta[oA + i];
                for (size_t i = 0; i < InputSize; i++) inputB[i] += delta[oB + i];
#endif
            }

            /// \brief Subtract the delta from elements in the input arrays.
            /// \tparam T The type of the input and delta.
            /// \tparam InputSize The size of the input arrays.
            /// \tparam DeltaSize The size of the delta array.
            /// \param inputA The first input array.
            /// \param inputB The second input array.
            /// \param delta The delta array.
            /// \param oA The delta offset for the first input array.
            /// \param oB The delta offset for the second input array.
            /// \details This function subtracts the offset delta from elements in the input arrays.
            ///          The delta is subtracted from the input arrays in-place.
            template<typename T, size_t InputSize, size_t DeltaSize>
            static inline void SubtractFromAll(std::array<T, InputSize>& inputA, std::array<T, InputSize>& inputB,
                                               const std::array<T, DeltaSize>& delta,
                                               const uint32_t oA, const uint32_t oB)
            {
#ifdef __AVX512BW__
                // Define the registers used in the loops:
                Vec512I zmm0;
                Vec512I zmm1;

                // Define the step size for the loops:
                constexpr size_t Step = sizeof(Vec512I) / sizeof(T);

                //region INPUT A
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    zmm0 = Avx512<T>::From(inputA,      i);
                    zmm1 = Avx512<T>::From(delta , oA + i);

                    // Subtract the delta register from the input register:
                    zmm0 = Avx512<T>::Subtract(zmm0, zmm1);

                    // Store the result back from the input register to the input array:
                    Avx512<T>::Store(zmm0, inputA, i);
                }
                //endregion

                //region INPUT B
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    zmm0 = Avx512<T>::From(inputB,      i);
                    zmm1 = Avx512<T>::From(delta , oB + i);

                    // Subtract the delta register from the input register:
                    zmm0 = Avx512<T>::Subtract(zmm0, zmm1);

                    // Store the result back from the input register to the input array:
                    Avx512<T>::Store(zmm0, inputB, i);
                }
                //endregion
#elifdef __AVX2__
                // Define the registers used in the loops:
                Vec256I ymm0;
                Vec256I ymm1;

                // Define the step size for the loops:
                constexpr size_t Step = sizeof(Vec256I) / sizeof(T);

                //region INPUT A
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    ymm0 = Avx<T> ::From(inputA,      i);
                    ymm1 = Avx<T> ::From(delta , oA + i);

                    // Subtract the delta register from the input register:
                    ymm0 = Avx2<T>::Subtract(ymm0, ymm1);

                    // Store the result back from the input register to the input array:
                    Avx<T>::Store(ymm0, inputA, i);
                }
                //endregion

                //region INPUT B
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    ymm0 = Avx<T> ::From(inputB,      i);
                    ymm1 = Avx<T> ::From(delta , oB + i);

                    // Subtract the delta register from the input register:
                    ymm0 = Avx2<T>::Subtract(ymm0, ymm1);

                    // Store the result back from the input register to the input array:
                    Avx<T>::Store(ymm0, inputB, i);
                }
                //endregion
#else
                // Subtract the delta from the input arrays:
                for (size_t i = 0; i < InputSize; i++) inputA[i] -= delta[oA + i];
                for (size_t i = 0; i < InputSize; i++) inputB[i] -= delta[oB + i];
#endif
            }

            /// \brief Combination of SubtractFromAll and AddToAll.
            /// \tparam T The type of the input and delta.
            /// \tparam InputSize The size of the input arrays.
            /// \tparam DeltaSize The size of the delta array.
            /// \param inputA The first input array.
            /// \param inputB The second input array.
            /// \param delta The delta array.
            /// \param oAS The delta offset for the first input array with respect to subtraction.
            /// \param oAA The delta offset for the first input array with respect to addition.
            /// \param oBS The delta offset for the second input array with respect to subtraction.
            /// \param oBA The delta offset for the second input array with respect to addition.
            /// \details This function subtracts the offset delta from elements in the input arrays.
            ///          The delta is subtracted from the input arrays in-place.
            ///          Then another offset delta is added to the input arrays.
            ///          The delta is added to the input arrays in-place.
            template<typename T, size_t InputSize, size_t DeltaSize>
            static inline void SubtractAndAddToAll(std::array<T, InputSize>& inputA, std::array<T, InputSize>& inputB,
                                                   const std::array<T, DeltaSize>& delta,
                                                   const uint32_t oAS, const uint32_t oAA,
                                                   const uint32_t oBS, const uint32_t oBA)
            {
#ifdef __AVX512BW__
                // Define the registers used in the loops:
                Vec512I zmm0;
                Vec512I zmm1;
                Vec512I zmm2;

                // Define the step size for the loops:
                constexpr size_t Step = sizeof(Vec512I) / sizeof(T);

                //region INPUT A
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    zmm0 = Avx512<T>::From(inputA,       i);
                    zmm1 = Avx512<T>::From(delta , oAS + i);
                    zmm2 = Avx512<T>::From(delta , oAA + i);

                    // Subtract and add the delta register to the input register:
                    zmm0 = Avx512<T>::Subtract(zmm0, zmm1);
                    zmm0 = Avx512<T>::Add(zmm0, zmm2);

                    // Store the result back from the input register to the input array:
                    Avx512<T>::Store(zmm0, inputA, i);
                }
                //endregion

                //region INPUT B
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    zmm0 = Avx512<T>::From(inputB,       i);
                    zmm1 = Avx512<T>::From(delta , oBS + i);
                    zmm2 = Avx512<T>::From(delta , oBA + i);

                    // Subtract and add the delta register to the input register:
                    zmm0 = Avx512<T>::Subtract(zmm0, zmm1);
                    zmm0 = Avx512<T>::Add(zmm0, zmm2);

                    // Store the result back from the input register to the input array:
                    Avx512<T>::Store(zmm0, inputB, i);
                }
                //endregion
#elifdef __AVX2__
                // Define the registers used in the loops:
                Vec256I ymm0;
                Vec256I ymm1;
                Vec256I ymm2;

                // Define the step size for the loops:
                constexpr size_t Step = sizeof(Vec256I) / sizeof(T);

                //region INPUT A
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    ymm0 = Avx<T> ::From(inputA,       i);
                    ymm1 = Avx<T> ::From(delta , oAS + i);
                    ymm2 = Avx<T> ::From(delta , oAA + i);

                    // Subtract and add the delta registers to the input register:
                    ymm0 = Avx2<T>::Subtract(ymm0, ymm1);
                    ymm0 = Avx2<T>::Add(ymm0, ymm2);

                    // Store the result back from the input register to the input array:
                    Avx<T>::Store(ymm0, inputA, i);
                }
                //endregion

                //region INPUT B
                for (size_t i = 0; i < InputSize; i += Step) {
                    // Load the input and delta values into the registers:
                    ymm0 = Avx<T> ::From(inputB,       i);
                    ymm1 = Avx<T> ::From(delta , oBS + i);
                    ymm2 = Avx<T> ::From(delta , oBA + i);

                    // Subtract and add the delta registers to the input register:
                    ymm0 = Avx2<T>::Subtract(ymm0, ymm1);
                    ymm0 = Avx2<T>::Add(ymm0, ymm2);

                    // Store the result back from the input register to the input array:
                    Avx<T>::Store(ymm0, inputB, i);
                }
                //endregion
#else
                // Subtract and add the delta to the input arrays:
                for (size_t i = 0; i < InputSize; i++) {
                    inputA[i] = inputA[i] - delta[oAS + i] + delta[oAA + i];
                    inputB[i] = inputB[i] - delta[oBS + i] + delta[oBA + i];
                }
#endif
            }

            /// \brief Activate the input arrays, flatten the concatenated tensor result, and forward propagate the
            ///        flattened result.
            /// \tparam Activation The activation function to use.
            /// \tparam T The type of the input, weight, and bias arrays.
            /// \tparam OT The type of the output array.
            /// \tparam InputSize The size of the input arrays.
            /// \tparam OutputSize The size of the output array.
            /// \param inputA The first input array.
            /// \param inputB The second input array.
            /// \param weight The weight array.
            /// \param bias The bias array.
            /// \param output The output array.
            /// \param o The offset into the output array.
            /// \details This function activates the input arrays. Then it creates a Tensor-view of the input arrays,
            ///          concatenating them vertically. After which, it flattens the vertical tensor into a 1D tensor.
            ///          Finally, it forwards propagates the flattened tensor with respect to the weight and bias arrays
            ///          using simple matrix multiplication. The result is stored in the output array starting at the
            ///          given offset.
            template<typename Activation, typename T, typename OT, size_t InputSize, size_t OutputSize>
            [[clang::noinline]]
            static void ActivateFlattenAndForward(
                    const std::array<T, InputSize>& inputA, const std::array<T, InputSize>& inputB,
                    const std::array<T, InputSize * 2 * OutputSize>& weight,
                    const std::array<T, OutputSize>& bias,
                    std::array<OT, OutputSize>& output, const uint32_t o)
            {
                // Define the stride with respect to the weight array:
                size_t stride = 0;

                // Perform the joint activation-flattening-forward propagation using matrix multiplication, defined as
                // output = activation(flatten(input)) * weight + bias:
                for (size_t i = 0; i < OutputSize; i++) {
#ifdef __AVX512BW__
                    // Define the register for sum accumulation:
                    Vec512I zmm0 = Avx512<OT>::Zero();

                    // Define the registers used in the inner loop:
                    Vec512I zmm1;
                    Vec512I zmm2;

                    // Define the step size for the loop:
                    constexpr size_t Step = sizeof(Vec512I) / sizeof(T);

                    // Inner loop performing sum += activation(flatten(input)) * weight:
                    for (size_t j = 0; j < InputSize; j += Step) {
                        //region INPUT A
                        // Load the input array and weight array into registers:
                        zmm1 = Avx512<T> ::From(inputA, j);
                        zmm2 = Avx512<T> ::From(weight, stride + j);

                        // Activate the input register:
                        zmm1 = Activation::Activate(zmm1);

                        // Multiply the input register by the weight register and add the result to the sum register,
                        // performing sum += input * weight:
                        zmm1 = Avx512<T> ::MultiplyAndAddAdjacent(zmm1, zmm2);
                        zmm0 = Avx512<OT>::Add(zmm0, zmm1);
                        //endregion

                        //region INPUT B
                        // Load the input array and weight array into registers:
                        zmm1 = Avx512<T> ::From(inputB, j);
                        zmm2 = Avx512<T> ::From(weight, InputSize + stride + j);

                        // Activate the input register:
                        zmm1 = Activation::Activate(zmm1);

                        // Multiply the input register by the weight register and add the result to the sum register,
                        // performing sum += input * weight:
                        zmm1 = Avx512<T> ::MultiplyAndAddAdjacent(zmm1, zmm2);
                        zmm0 = Avx512<OT>::Add(zmm0, zmm1);
                        //endregion
                    }

                    stride += InputSize * 2;

                    output[o + i] = Avx512<OT>::Sum(zmm0) + bias[o + i];
#elifdef __AVX2__
                    // Define the register for sum accumulation:
                    Vec256I ymm0 = Avx<OT>::Zero();

                    // Define the registers used in the inner loop:
                    Vec256I ymm1;
                    Vec256I ymm2;

                    // Define the step size for the loop:
                    constexpr size_t Step = sizeof(Vec256I) / sizeof(T);

                    // Inner loop performing sum += activation(flatten(input)) * weight:
                    for (size_t j = 0; j < InputSize; j += Step) {
                        //region INPUT A
                        // Load the input array and weight array into registers:
                        ymm1 = Avx<T>    ::From(inputA,          j);
                        ymm2 = Avx<T>    ::From(weight, stride + j);

                        // Activate the input register:
                        ymm1 = Activation::Activate(ymm1);

                        // Multiply the input register by the weight register and add the result to the sum register,
                        // performing sum += input * weight:
                        ymm1 = Avx2<T>   ::MultiplyAndAddAdjacent(ymm1, ymm2);
                        ymm0 = Avx2<OT>  ::Add(ymm0, ymm1);
                        //endregion

                        //region INPUT B
                        // Load the input array and weight array into registers:
                        ymm1 = Avx<T>    ::From(inputB,                      j);
                        ymm2 = Avx<T>    ::From(weight, InputSize + stride + j);

                        // Activate the input register:
                        ymm1 = Activation::Activate(ymm1);

                        // Multiply the input register by the weight register and add the result to the sum register,
                        // performing sum += input * weight:
                        ymm1 = Avx2<T>   ::MultiplyAndAddAdjacent(ymm1, ymm2);
                        ymm0 = Avx2<OT>  ::Add(ymm0, ymm1);
                        //endregion
                    }

                    // Stride to the next set of weights:
                    stride += InputSize * 2;

                    // Sum up the sum accumulation register and store the result with respect to the bias:
                    output[o + i] = Avx2<OT>::Sum(ymm0) + bias[o + i];
#else
                    // Define the sum accumulation variable:
                    OT sum = 0;

                    for (size_t j = 0; j < InputSize; j++) {
                        // Add the activation of the input multiplied by the weight to the sum:
                        sum += Activation::Activate(inputA[j]) * weight[stride + j];
                        sum += Activation::Activate(inputB[j]) * weight[InputSize + stride + j];
                    }

                    // Stride to the next set of weights:
                    stride += InputSize * 2;

                    // Store the sum with respect to the bias:
                    output[o + i] = sum + bias[o + i];
#endif
                }
            }

    };
} // MantaRay

#endif //MANTARAY_SIMD_H
