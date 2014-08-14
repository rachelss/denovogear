/*
 * Copyright (c) 2014 Reed A. Cartwright
 * Authors:  Reed A. Cartwright <reed@cartwrig.ht>
 *
 * This file is part of DeNovoGear.
 *
 * DeNovoGear is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef DNG_PEDIGREE_H
#define DNG_PEDIGREE_H

#include <functional>

#include <dng/matrix.h>
#include <dng/io/ped.h>
#include <dng/newick.h>
#include <dng/read_group.h>

namespace dng {

class Pedigree {
public:
	bool Initialize(double theta, double mu);

	bool Construct(const io::Pedigree& pedigree, const dng::ReadGroups& rgs);
	
	double CalculateLogLikelihood(IndividualBuffer penetrances) {
		// TODO: Assert that length of penetrances is equal to num_members_;
		// Copy Penetrance values into the lower buffer
		// TODO: Eliminate this copy????
		lower_ = std::move(penetrances);
		// Copy genotype Priors
		// TODO: Only update founders
		// TODO: use a different prior based on reference
		upper_.assign(num_nodes_, genotype_prior_);
		
		// Peel pedigree one family at a time
		for(std::size_t i = 0; i < peeling_op_.size(); ++i)
			peeling_op_[i](this, family_members_[i]);
			
		// Sum over roots
		double ret = 0.0;
		for(auto r : roots_) {
			ret += log((lower_[r]*upper_[r]).sum());
		}
		return ret;
	}
	
	std::size_t size() const { return num_nodes_; }

protected:
	typedef std::vector<size_t> family_members_t;

	std::vector<family_members_t> family_members_;
	std::size_t num_members_, num_libraries_, num_nodes_;
	std::vector<std::size_t> roots_;
	
	IndividualBuffer upper_; // Holds P(Data & G=g)
	IndividualBuffer lower_; // Holds P(Data | G=g)
	
	Vector10d genotype_prior_; // Holds P(G | theta)

	MeiosisMatrix meiosis_;
	Matrix10d mitosis_;

	Vector100d buffer_;

	void PeelUp(const family_members_t &family_members) {
		lower_[family_members[0]] *= (mitosis_ * lower_[family_members[1]].matrix()).array();
	}

	void PeelDown(const family_members_t &family_members) {
		upper_[family_members[1]].matrix() = mitosis_.transpose() *
			(upper_[family_members[0]]*lower_[family_members[0]]).matrix();
	}


	void PeelToFather(const family_members_t &family_members) {
		buffer_.setOnes();
		// Sum over children
		for(std::size_t i = 2; i < family_members.size(); i++) {
			buffer_ *= (meiosis_ * lower_[family_members[i]].matrix()).array();
		}
		// Include Mom
		Eigen::Map<Matrix10d, Eigen::Aligned> mat(buffer_.data());
		lower_[family_members[0]] *= (mat *
			(upper_[family_members[1]]*lower_[family_members[1]]).matrix()).array();
	}

	void PeelToMother(const family_members_t &family_members) {
		buffer_.setOnes();
		// Sum over children
		for(std::size_t i = 2; i < family_members.size(); i++) {
			buffer_ *= (meiosis_ * lower_[family_members[i]].matrix()).array();
		}
		// Include Dad
		Eigen::Map<RowMatrix10d, Eigen::Aligned> mat(buffer_.data());
		lower_[family_members[1]] *= (mat *
			(upper_[family_members[0]]*lower_[family_members[0]]).matrix()).array();
	}

	void PeelToChild(const family_members_t &family_members) {
		buffer_.setOnes();
		// Sum over children
		for(std::size_t i = 3; i < family_members.size(); i++) {
			buffer_ *= (meiosis_ * lower_[family_members[i]].matrix()).array();
		}
		// Parents
		buffer_ *= kroneckerProduct(
			(lower_[family_members[0]] * upper_[family_members[0]]).matrix(),
			(lower_[family_members[1]] * upper_[family_members[1]]).matrix()
		).array();
		
		upper_[family_members[2]].matrix() = meiosis_.transpose() * buffer_.matrix();
	}
	typedef Pedigree Op;

	typedef decltype(std::mem_fn(&Pedigree::PeelToFather)) PeelOp;

	std::vector<PeelOp> peeling_op_;	
};

}; // namespace dng


#endif // DNG_PEDIGREE_H
