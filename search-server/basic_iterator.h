#pragma once

template <typename Type, typename InternalIterator, typename ExternalIterator>
class BasicIterator {
public:
	using iterator_category = std::forward_iterator_tag;
	using value_type = Type;
	using difference_type = std::ptrdiff_t;
	using pointer = value_type*;
	using reference = value_type&;

	BasicIterator() = default;

	explicit BasicIterator(ExternalIterator external_it, ExternalIterator external_end)
		:
		external_it_(external_it),
		external_end_(external_end) {
		SetInternalItBegin();
	}

	const reference operator * () const noexcept {
		return *internal_it_;
	}

	[[nodiscard]] bool operator==(const BasicIterator& rhs) const noexcept {
		if (external_it_ == external_end_) {
			return external_end_ == rhs.external_it_;
		}
		else {
			return  internal_it_ == rhs.internal_it_ && external_it_ == rhs.external_it_;
		}
	}

	[[nodiscard]] bool operator!=(const BasicIterator& rhs) const noexcept {
		return !(*this == rhs);
	}

	BasicIterator& operator++() noexcept {
		Plus();
		return *this;
	}

private:
	ExternalIterator external_it_;
	ExternalIterator external_end_;
	InternalIterator internal_it_;

	void SetInternalItBegin() {
		while (external_it_ != external_end_) {
			internal_it_ = external_it_->begin();
			if (internal_it_ == external_it_->end()) {
				++external_it_;
			}
			else {
				break;
			}
		}
	}

	void Plus() {
		++internal_it_;
		if (internal_it_ == external_it_->end()) {
			++external_it_;
			SetInternalItBegin();
		}
	}
};