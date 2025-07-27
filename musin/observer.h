#pragma once

#include <utility>

/**
 * @brief A compile-time, zero-cost observer pattern implementation.
 * 
 * This library provides an alternative to runtime observer patterns like etl::observer.
 * Its primary goal is to move the observer-subject relationship from runtime to compile time,
 * yielding significant benefits in performance and memory usage.
 * 
 * Core Benefits:
 * 1.  **Zero RAM Overhead**: The list of observers is not stored in a runtime container.
 *     The connections are resolved at compile time, consuming no RAM.
 * 2.  **Zero CPU Overhead**: Notifying observers compiles down to a series of direct,
 *     statically-dispatched function calls. There is no loop, no pointer indirection,
 *     and no virtual function call overhead.
 * 3.  **Compile-Time Safety**: It is impossible for an observer to be "forgotten" or for
 *     a notification to be sent to a null pointer. All connections are validated by the compiler.
 * 
 * Key Trade-Off:
 * -   **No Runtime Flexibility**: Observers cannot be added or removed at runtime. The entire
 *     event routing graph is fixed when the code is compiled. This makes this pattern ideal
 *     for static, well-defined relationships, such as between hardware drivers and UI components,
 *     but unsuitable for dynamic plugin systems.
 * 
 * API Compatibility with etl::observer:
 * -   The `musin::observer` class is designed to feel familiar. A class can inherit from it
 *     to signal its intent to observe certain event types.
 * -   The `musin::observable` class provides the `notify_observers` method.
 */
namespace musin
{
  /**
   * @brief Base class for observers to provide a familiar API.
   * @tparam TTypes The event types this observer can handle.
   * 
   * This class serves as a marker to indicate what events an observer subscribes to.
   * It uses no virtual functions; dispatch is handled statically by the observable.
   * A class should implement a public `notification(T)` method for each type `T` listed.
   */
  template <typename... TTypes>
  class observer;

  template <typename T1, typename... TRest>
  class observer<T1, TRest...> : public observer<T1>, public observer<TRest...>
  {
  public:
    using observer<T1>::notification;
    using observer<TRest...>::notification;
  };

  template <typename T1>
  class observer<T1>
  {
  public:
    // User is expected to implement: void notification(T1);
  };

  template <>
  class observer<void>
  {
  public:
    // User is expected to implement: void notification();
  };

  /**
   * @brief An observable subject that notifies a compile-time list of observers.
   * @tparam Observers A parameter pack of pointers to observer objects with static linkage.
   * 
   * This class has no member variables and thus no memory footprint. All its operations
   * are static and resolved at compile time.
   */
  template <auto*... Observers>
  class observable
  {
  public:
    observable() = delete; // This class should not be instantiated.

    /**
     * @brief Notify all registered observers of an event.
     * @tparam T The type of the event message.
     * @param message The event message to send.
     * 
     * This function expands at compile time to a series of direct calls to the
     * `notification` method of each observer.
     */
    template <typename T>
    static constexpr void notify_observers(const T& message)
    {
      (Observers->notification(message), ...);
    }

    /**
     * @brief Notify all registered observers of a parameter-less event.
     * 
     * This function expands at compile time to a series of direct calls to the
     * `notification` method of each observer.
     */
    static constexpr void notify_observers()
    {
      (Observers->notification(), ...);
    }
  };

} // namespace musin
