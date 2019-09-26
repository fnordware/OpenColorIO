// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <OpenColorIO/OpenColorIO.h>

#include <algorithm>

#include "LookParse.h"
#include "ParseUtils.h"
#include "pystring/pystring.h"
#include <iostream>

OCIO_NAMESPACE_ENTER
{
    void LookParseResult::Token::parse(const std::string & str)
    {
        // Assert no commas, colons, or | in str.
        
        if(pystring::startswith(str, "+"))
        {
            name = pystring::lstrip(str, "+");
            dir = TRANSFORM_DIR_FORWARD;
        }
        // TODO: Handle --
        else if(pystring::startswith(str, "-"))
        {
            name = pystring::lstrip(str, "-");
            dir = TRANSFORM_DIR_INVERSE;
        }
        else
        {
            name = str;
            dir = TRANSFORM_DIR_FORWARD;
        }
    }
    
    void LookParseResult::Token::serialize(std::ostream & os) const
    {
        if(dir==TRANSFORM_DIR_FORWARD)
        {
            os << name;
        }
        else if(dir==TRANSFORM_DIR_INVERSE)
        {
            os << "-" << name;
        }
        else
        {
            os << "?" << name;
        }
    }
    
    void LookParseResult::serialize(std::ostream & os, const Tokens & tokens)
    {
        for(unsigned int i=0; i<tokens.size(); ++i)
        {
            if(i!=0) os << ", ";
            tokens[i].serialize(os);
        }
    }
    
    const LookParseResult::Options & LookParseResult::parse(const std::string & looksstr)
    {
        m_options.clear();
        
        std::string strippedlooks = pystring::strip(looksstr);
        if(strippedlooks.empty())
        {
            return m_options;
        }
        
        StringVec options;
        pystring::split(strippedlooks, options, "|");
        
        StringVec vec;
        
        for(unsigned int optionsindex=0;
            optionsindex<options.size();
            ++optionsindex)
        {
            LookParseResult::Tokens tokens;
            
            vec.clear();
            SplitStringEnvStyle(vec, options[optionsindex].c_str());
            for(unsigned int i=0; i<vec.size(); ++i)
            {
                LookParseResult::Token t;
                t.parse(vec[i]);
                tokens.push_back(t);
            }
            
            m_options.push_back(tokens);
        }
        
        return m_options;
    }
    
    const LookParseResult::Options & LookParseResult::getOptions() const
    {
        return m_options;
    }
    
    bool LookParseResult::empty() const
    {
        return m_options.empty();
    }
    
    void LookParseResult::reverse()
    {
        // m_options itself should NOT be reversed.
        // The individual looks
        // need to be applied in the inverse direction. But, the precedence
        // for which option to apply is to be maintained!
        
        for(unsigned int optionindex=0;
            optionindex<m_options.size();
            ++optionindex)
        {
            std::reverse(m_options[optionindex].begin(), m_options[optionindex].end());
            
            for(unsigned int tokenindex=0;
                tokenindex<m_options[optionindex].size();
                ++tokenindex)
            {
                m_options[optionindex][tokenindex].dir = 
                    GetInverseTransformDirection(
                        m_options[optionindex][tokenindex].dir);
            }
        }
    }
}
OCIO_NAMESPACE_EXIT



///////////////////////////////////////////////////////////////////////////////

#ifdef OCIO_UNIT_TEST

OCIO_NAMESPACE_USING

#include "UnitTest.h"

OCIO_ADD_TEST(LookParse, Parse)
{
    LookParseResult r;
    
    {
    const LookParseResult::Options & options = r.parse("");
    OCIO_CHECK_EQUAL(options.size(), 0);
    OCIO_CHECK_EQUAL(options.empty(), true);
    }
    
    {
    const LookParseResult::Options & options = r.parse("  ");
    OCIO_CHECK_EQUAL(options.size(), 0);
    OCIO_CHECK_EQUAL(options.empty(), true);
    }
    
    {
    const LookParseResult::Options & options = r.parse("cc");
    OCIO_CHECK_EQUAL(options.size(), 1);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options.empty(), false);
    }
    
    {
    const LookParseResult::Options & options = r.parse("+cc");
    OCIO_CHECK_EQUAL(options.size(), 1);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options.empty(), false);
    }
    
    {
    const LookParseResult::Options & options = r.parse("  +cc");
    OCIO_CHECK_EQUAL(options.size(), 1);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options.empty(), false);
    }
    
    {
    const LookParseResult::Options & options = r.parse("  +cc   ");
    OCIO_CHECK_EQUAL(options.size(), 1);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options.empty(), false);
    }
    
    {
    const LookParseResult::Options & options = r.parse("+cc,-di");
    OCIO_CHECK_EQUAL(options.size(), 1);
    OCIO_CHECK_EQUAL(options[0].size(), 2);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options[0][1].name, "di");
    OCIO_CHECK_EQUAL(options[0][1].dir, TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_EQUAL(options.empty(), false);
    }
    
    {
    const LookParseResult::Options & options = r.parse("  +cc ,  -di");
    OCIO_CHECK_EQUAL(options.size(), 1);
    OCIO_CHECK_EQUAL(options[0].size(), 2);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options[0][1].name, "di");
    OCIO_CHECK_EQUAL(options[0][1].dir, TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_EQUAL(options.empty(), false);
    }
    
    {
    const LookParseResult::Options & options = r.parse("  +cc :  -di");
    OCIO_CHECK_EQUAL(options.size(), 1);
    OCIO_CHECK_EQUAL(options[0].size(), 2);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options[0][1].name, "di");
    OCIO_CHECK_EQUAL(options[0][1].dir, TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_EQUAL(options.empty(), false);
    }
    
    {
    const LookParseResult::Options & options = r.parse("+cc, -di |-cc");
    OCIO_CHECK_EQUAL(options.size(), 2);
    OCIO_CHECK_EQUAL(options[0].size(), 2);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options[0][1].name, "di");
    OCIO_CHECK_EQUAL(options[0][1].dir, TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_EQUAL(options[1].size(), 1);
    OCIO_CHECK_EQUAL(options.empty(), false);
    OCIO_CHECK_EQUAL(options[1][0].name, "cc");
    OCIO_CHECK_EQUAL(options[1][0].dir, TRANSFORM_DIR_INVERSE);
    }
    
    {
    const LookParseResult::Options & options = r.parse("+cc, -di |-cc|   ");
    OCIO_CHECK_EQUAL(options.size(), 3);
    OCIO_CHECK_EQUAL(options[0].size(), 2);
    OCIO_CHECK_EQUAL(options[0][0].name, "cc");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options[0][1].name, "di");
    OCIO_CHECK_EQUAL(options[0][1].dir, TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_EQUAL(options[1].size(), 1);
    OCIO_CHECK_EQUAL(options.empty(), false);
    OCIO_CHECK_EQUAL(options[1][0].name, "cc");
    OCIO_CHECK_EQUAL(options[1][0].dir, TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_EQUAL(options[2].size(), 1);
    OCIO_CHECK_EQUAL(options[2][0].name, "");
    OCIO_CHECK_EQUAL(options[2][0].dir, TRANSFORM_DIR_FORWARD);
    }
}

OCIO_ADD_TEST(LookParse, Reverse)
{
    LookParseResult r;
    
    {
    r.parse("+cc, -di |-cc|   ");
    r.reverse();
    const LookParseResult::Options & options = r.getOptions();
    
    OCIO_CHECK_EQUAL(options.size(), 3);
    OCIO_CHECK_EQUAL(options[0].size(), 2);
    OCIO_CHECK_EQUAL(options[0][1].name, "cc");
    OCIO_CHECK_EQUAL(options[0][1].dir, TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_EQUAL(options[0][0].name, "di");
    OCIO_CHECK_EQUAL(options[0][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options[1].size(), 1);
    OCIO_CHECK_EQUAL(options.empty(), false);
    OCIO_CHECK_EQUAL(options[1][0].name, "cc");
    OCIO_CHECK_EQUAL(options[1][0].dir, TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(options[2].size(), 1);
    OCIO_CHECK_EQUAL(options[2][0].name, "");
    OCIO_CHECK_EQUAL(options[2][0].dir, TRANSFORM_DIR_INVERSE);
    }

    
}

#endif // OCIO_UNIT_TEST
