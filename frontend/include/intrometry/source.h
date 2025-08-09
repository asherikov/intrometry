/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Source parameters and necessary ariles inclusions.
*/

#pragma once

#include <ariles2/ariles.h>


namespace intrometry
{
    /**
     * @brief Data source (parameters).
     *
     * Currently contains only parameters that can be used when assigning a
     * bare ariles class to an intrometry sink.
     *
     * @ingroup API
     */
    class Source
    {
    public:
        class Parameters
        {
        public:
            /**
             * If true assume that the number and order of entries is
             * preserved, if false new names are generated and published on
             * each write (it is unlikely that you want this). It is not
             * possible to detect persistent structure automatically, since
             * variable containers may have constant size. Wrong setting is not
             * going to result in an error, but metric IDs and values may not
             * match.
             */
            bool persistent_structure_;

        public:
            explicit Parameters(const bool persistent_structure = false)
            {
                persistent_structure_ = persistent_structure;
            }

            Parameters &persistent_structure(const bool value)
            {
                persistent_structure_ = value;
                return (*this);
            }
        };
    };
}  // namespace intrometry
